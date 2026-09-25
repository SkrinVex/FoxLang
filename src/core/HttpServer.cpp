#include "HttpServer.h"
#include "foxlang/AST.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace foxlang::http {
namespace fs = std::filesystem;

namespace {

int hexDigit(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return text;
}

std::string trim(const std::string& text) {
    auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

// A parameter of a header value such as boundary=... or name="...".
std::string headerParameter(const std::string& header, const std::string& name) {
    std::string lowered = lower(header);
    for (size_t at = 0; (at = lowered.find(name + "=", at)) != std::string::npos; at += name.size()) {
        if (at > 0 && lowered[at - 1] != ';' && lowered[at - 1] != ' ' && lowered[at - 1] != '\t') continue;
        size_t start = at + name.size() + 1;
        if (start < header.size() && header[start] == '"') {
            size_t end = header.find('"', start + 1);
            return header.substr(start + 1, end == std::string::npos ? std::string::npos : end - start - 1);
        }
        size_t end = header.find(';', start);
        return trim(header.substr(start, end == std::string::npos ? std::string::npos : end - start));
    }
    return "";
}

std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> segments;
    std::stringstream stream(path);
    std::string segment;
    while (std::getline(stream, segment, '/'))
        if (!segment.empty()) segments.push_back(percentDecode(segment, false));
    return segments;
}

bool matches(const platform::HttpRoute& route, const std::vector<std::string>& segments,
             std::map<std::string, std::string>& params) {
    std::map<std::string, std::string> found;
    for (size_t i = 0; i < route.segments.size(); ++i) {
        const std::string& part = route.segments[i];
        if (!part.empty() && part[0] == '*') {
            std::string rest;
            for (size_t j = i; j < segments.size(); ++j) rest += (j > i ? "/" : "") + segments[j];
            found[part.size() > 1 ? part.substr(1) : "*"] = rest;
            params = std::move(found);
            return true;
        }
        if (i >= segments.size()) return false;
        if (!part.empty() && part[0] == ':') found[part.substr(1)] = segments[i];
        else if (part != segments[i]) return false;
    }
    if (route.segments.size() != segments.size()) return false;
    params = std::move(found);
    return true;
}

bool isPattern(const platform::HttpRoute& route) {
    return route.pattern.find(':') != std::string::npos || route.pattern.find('*') != std::string::npos;
}

// A file under a static mount. Hidden files and parent references are never served:
// a mounted project directory must not give away .env or files outside it.
enum class StaticResult { NotFound, Served, Redirect };

StaticResult serveStatic(const std::string& directory, const std::vector<std::string>& rest,
                         const std::string& requestPath, platform::HttpReply& reply) {
    fs::path file = fs::u8path(directory);
    for (const auto& segment : rest) {
        if (segment.empty() || segment[0] == '.' || segment.find_first_of("\\:\0", 0, 3) != std::string::npos)
            return StaticResult::NotFound;
        file /= fs::u8path(segment);
    }
    std::error_code ec;
    if (fs::is_directory(file, ec)) {
        if (requestPath.empty() || requestPath.back() != '/') {
            reply.status = 301;
            reply.headers.push_back({"Location", requestPath + "/"});
            reply.body.clear();
            return StaticResult::Redirect;
        }
        file /= "index.html";
    }
    return sendFile(reply, file.u8string()) ? StaticResult::Served : StaticResult::NotFound;
}

void invokeHandler(const Value& handler, Context& rootCtx, platform::HttpReply& reply) {
    Value function = handler;
    if (handler.isString()) {
        auto definition = rootCtx.getFunc(handler.str());
        if (!definition) {
            std::cerr << "[HTTP ERROR] Handler function '" << handler.str() << "' not found" << std::endl;
            reply = {500, "application/json; charset=utf-8", "{\"error\":\"Handler not found\"}", {}};
            return;
        }
        auto callee = std::make_shared<Callee>();
        callee->name = handler.str();
        callee->function = definition;
        function = Value::container(Value::Kind::Function);
        function.ref()->callee = std::move(callee);
    }
    try {
        runtime::callValue(function, nullptr, 0, rootCtx);
    } catch (const std::exception& error) {
        std::cerr << "[HTTP ERROR] " << runtime::locate(error.what(), "") << std::endl;
        reply = {500, "application/json; charset=utf-8", "{\"error\":\"Handler failed\"}", {}};
    }
}

} // namespace

std::string percentDecode(const std::string& text, bool plusIsSpace) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%' && i + 2 < text.size() && hexDigit(text[i + 1]) >= 0 && hexDigit(text[i + 2]) >= 0) {
            out += static_cast<char>(hexDigit(text[i + 1]) * 16 + hexDigit(text[i + 2]));
            i += 2;
        } else if (plusIsSpace && text[i] == '+') {
            out += ' ';
        } else {
            out += text[i];
        }
    }
    return out;
}

std::string percentEncode(const std::string& text) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char ch : text) {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            out += static_cast<char>(ch);
        } else {
            out += '%';
            out += hex[ch >> 4];
            out += hex[ch & 15];
        }
    }
    return out;
}

std::string htmlEscape(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        switch (ch) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += ch;
        }
    }
    return out;
}

std::map<std::string, std::string> parseQuery(const std::string& text) {
    std::map<std::string, std::string> values;
    std::stringstream stream(text);
    std::string pair;
    while (std::getline(stream, pair, '&')) {
        if (pair.empty()) continue;
        auto equals = pair.find('=');
        std::string key = percentDecode(pair.substr(0, equals), true);
        std::string value = equals == std::string::npos ? "" : percentDecode(pair.substr(equals + 1), true);
        values.emplace(std::move(key), std::move(value));
    }
    return values;
}

std::vector<Part> parseMultipart(const platform::HttpRequest& request) {
    std::vector<Part> parts;
    auto type = request.headers.find("content-type");
    if (type == request.headers.end() || lower(type->second).rfind("multipart/form-data", 0) != 0) return parts;
    std::string boundary = headerParameter(type->second, "boundary");
    if (boundary.empty()) return parts;
    const std::string& body = request.body;
    std::string delimiter = "--" + boundary;
    size_t at = body.find(delimiter);
    while (at != std::string::npos) {
        at += delimiter.size();
        if (body.compare(at, 2, "--") == 0) break; // closing delimiter
        if (body.compare(at, 2, "\r\n") == 0) at += 2;
        size_t headersEnd = body.find("\r\n\r\n", at);
        if (headersEnd == std::string::npos) break;
        size_t next = body.find("\r\n" + delimiter, headersEnd + 4);
        if (next == std::string::npos) break;
        Part part;
        std::stringstream headers(body.substr(at, headersEnd - at));
        std::string line;
        while (std::getline(headers, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto colon = line.find(':');
            if (colon == std::string::npos) continue;
            std::string name = lower(trim(line.substr(0, colon)));
            std::string value = trim(line.substr(colon + 1));
            if (name == "content-disposition") {
                part.name = headerParameter(value, "name");
                part.filename = headerParameter(value, "filename");
            } else if (name == "content-type") {
                part.contentType = value;
            }
        }
        part.data = body.substr(headersEnd + 4, next - headersEnd - 4);
        parts.push_back(std::move(part));
        at = next + 2;
    }
    return parts;
}

std::string formValue(const platform::HttpRequest& request, const std::string& name) {
    for (const auto& part : parseMultipart(request))
        if (part.name == name && part.filename.empty()) return part.data;
    auto type = request.headers.find("content-type");
    if (type != request.headers.end() && lower(type->second).rfind("application/x-www-form-urlencoded", 0) == 0) {
        auto values = parseQuery(request.body);
        auto found = values.find(name);
        if (found != values.end()) return found->second;
    }
    return "";
}

std::map<std::string, std::string> parseCookies(const std::string& header) {
    std::map<std::string, std::string> cookies;
    std::stringstream stream(header);
    std::string pair;
    while (std::getline(stream, pair, ';')) {
        auto equals = pair.find('=');
        if (equals == std::string::npos) continue;
        cookies.emplace(trim(pair.substr(0, equals)), percentDecode(trim(pair.substr(equals + 1)), false));
    }
    return cookies;
}

std::string mimeType(const std::string& path) {
    static const std::map<std::string, std::string> types = {
        {".html", "text/html; charset=utf-8"}, {".htm", "text/html; charset=utf-8"},
        {".css", "text/css; charset=utf-8"}, {".js", "text/javascript; charset=utf-8"},
        {".mjs", "text/javascript; charset=utf-8"}, {".json", "application/json; charset=utf-8"},
        {".txt", "text/plain; charset=utf-8"}, {".md", "text/markdown; charset=utf-8"},
        {".csv", "text/csv; charset=utf-8"}, {".xml", "application/xml; charset=utf-8"},
        {".svg", "image/svg+xml"}, {".png", "image/png"}, {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"},
        {".gif", "image/gif"}, {".webp", "image/webp"}, {".ico", "image/x-icon"}, {".avif", "image/avif"},
        {".pdf", "application/pdf"}, {".zip", "application/zip"}, {".wasm", "application/wasm"},
        {".woff", "font/woff"}, {".woff2", "font/woff2"}, {".ttf", "font/ttf"}, {".otf", "font/otf"},
        {".mp3", "audio/mpeg"}, {".ogg", "audio/ogg"}, {".wav", "audio/wav"},
        {".mp4", "video/mp4"}, {".webm", "video/webm"}, {".fox", "text/plain; charset=utf-8"},
    };
    auto found = types.find(lower(fs::u8path(path).extension().u8string()));
    return found == types.end() ? "application/octet-stream" : found->second;
}

bool sendFile(platform::HttpReply& reply, const std::string& path, const std::string& contentType) {
    std::error_code ec;
    auto file = fs::u8path(path);
    if (!fs::is_regular_file(file, ec)) return false;
    std::ifstream stream(file, std::ios::binary);
    if (!stream) return false;
    std::ostringstream bytes;
    bytes << stream.rdbuf();
    reply.body = bytes.str();
    reply.contentType = contentType.empty() ? mimeType(path) : contentType;
    return true;
}

bool validHeaderText(const std::string& text) {
    return text.find_first_of(std::string("\r\n\0", 3)) == std::string::npos;
}

} // namespace foxlang::http

namespace foxlang::platform {

HttpReply handleHttpRequest(ServerState& state, Context& rootCtx) {
    using namespace foxlang::http;
    HttpRequest& request = state.request;
    state.reply = {200, "application/json; charset=utf-8", "", {}};
    HttpReply& reply = state.reply;
    auto segments = http::splitPath(request.path);
    bool head = request.method == "HEAD";

    auto withCors = [&](HttpReply result) {
        if (!state.corsOrigin.empty()) {
            result.headers.push_back({"Access-Control-Allow-Origin", state.corsOrigin});
            result.headers.push_back({"Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS"});
            result.headers.push_back({"Access-Control-Allow-Headers", "Content-Type, Authorization"});
            if (state.corsOrigin != "*") result.headers.push_back({"Vary", "Origin"});
        }
        return result;
    };

    // Exact paths win over patterns; patterns are tried in registration order.
    const HttpRoute* route = nullptr;
    std::vector<std::string> allowed;
    for (int pass = 0; pass < 2 && !route; ++pass) {
        for (const auto& candidate : state.routes) {
            if (isPattern(candidate) != (pass == 1)) continue;
            std::map<std::string, std::string> params;
            if (!matches(candidate, segments, params)) continue;
            if (candidate.method == request.method || (head && candidate.method == "GET")) {
                route = &candidate;
                request.params = std::move(params);
                break;
            }
            allowed.push_back(candidate.method);
        }
    }
    if (route) {
        invokeHandler(route->handler, rootCtx, reply);
        return withCors(reply);
    }
    if (request.method == "OPTIONS" && !state.corsOrigin.empty()) return withCors({204, "", "", {}});

    if (request.method == "GET" || head) {
        for (const auto& [prefix, directory] : state.staticMounts) {
            auto mount = http::splitPath(prefix);
            if (mount.size() > segments.size() || !std::equal(mount.begin(), mount.end(), segments.begin())) continue;
            std::vector<std::string> rest(segments.begin() + static_cast<std::ptrdiff_t>(mount.size()), segments.end());
            if (serveStatic(directory, rest, request.path, reply) != StaticResult::NotFound) return withCors(reply);
        }
    }

    if (!allowed.empty()) {
        std::string list;
        for (const auto& method : allowed) list += (list.empty() ? "" : ", ") + method;
        return withCors({405, "application/json; charset=utf-8", "{\"error\":\"Method Not Allowed\"}", {{"Allow", list}}});
    }
    if (!state.notFoundHandler.isVoid()) {
        reply.status = 404;
        invokeHandler(state.notFoundHandler, rootCtx, reply);
        return withCors(reply);
    }
    return withCors({404, "application/json; charset=utf-8", "{\"error\":\"Not Found\"}", {}});
}

} // namespace foxlang::platform

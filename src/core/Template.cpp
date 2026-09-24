// Mustache-style templates over a JSON document, for HTML pages served by FoxLang.
//
//   {{path}}          value at the path, HTML-escaped
//   {{{path}}}        value at the path, as is
//   {{#each path}}    repeat for every element of an array (or value of an object);
//     {{.}}           the element itself, {{@index}} its position, {{@key}} its key
//   {{/each}}
//   {{#if path}} ... {{else}} ... {{/if}}   false, null, 0, "", [] and {} are false
//   {{! comment }}
//
// Inside a section a path is looked up in the element first, then outwards.
#include "foxlang/Runtime.h"
#include <memory>
#include <stdexcept>

namespace foxlang::runtime {
namespace {

struct TemplateNode {
    enum class Kind { Text, Escaped, Raw, Each, If } kind = Kind::Text;
    std::string text; // literal text, or the path of a tag or section
    std::vector<TemplateNode> body, otherwise;
};

class TemplateParser {
public:
    explicit TemplateParser(const std::string& source) : source(source) {}

    std::vector<TemplateNode> parse() {
        std::vector<TemplateNode> nodes;
        std::string closing = block(nodes, "");
        if (!closing.empty()) fail("unexpected {{" + closing + "}}");
        return nodes;
    }

private:
    const std::string& source;
    size_t pos = 0;

    [[noreturn]] void fail(const std::string& message) const {
        int line = 1;
        for (size_t i = 0; i < pos && i < source.size(); ++i) if (source[i] == '\n') ++line;
        throw std::runtime_error("Runtime Error: template line " + std::to_string(line) + ": " + message);
    }

    static std::string trim(const std::string& text) {
        auto first = text.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
    }

    // Reads nodes until a closing tag ({{/each}}, {{/if}} or {{else}}), which it returns.
    std::string block(std::vector<TemplateNode>& nodes, const std::string& section) {
        while (pos < source.size()) {
            size_t open = source.find("{{", pos);
            if (open == std::string::npos) open = source.size();
            if (open > pos) nodes.push_back({TemplateNode::Kind::Text, source.substr(pos, open - pos), {}, {}});
            pos = open;
            if (pos >= source.size()) break;
            bool triple = source.compare(pos, 3, "{{{") == 0;
            size_t close = source.find(triple ? "}}}" : "}}", pos);
            if (close == std::string::npos) fail("unclosed {{");
            std::string tag = trim(source.substr(pos + (triple ? 3 : 2), close - pos - (triple ? 3 : 2)));
            pos = close + (triple ? 3 : 2);
            if (triple) {
                nodes.push_back({TemplateNode::Kind::Raw, tag, {}, {}});
            } else if (tag.empty() || tag[0] == '!') {
                continue;
            } else if (tag.rfind("#each", 0) == 0 || tag.rfind("#if", 0) == 0) {
                bool each = tag[1] == 'e';
                TemplateNode node{each ? TemplateNode::Kind::Each : TemplateNode::Kind::If, trim(tag.substr(each ? 5 : 3)), {}, {}};
                if (node.text.empty()) fail("{{" + tag + "}} needs a path");
                std::string end = block(node.body, each ? "each" : "if");
                if (end == "else") {
                    if (each) fail("{{else}} is only allowed inside {{#if}}");
                    end = block(node.otherwise, "if");
                }
                if (end != (each ? "/each" : "/if")) fail("{{" + tag + "}} is not closed");
                nodes.push_back(std::move(node));
            } else if (tag == "/each" || tag == "/if" || tag == "else") {
                if (section.empty()) fail("unexpected {{" + tag + "}}");
                return tag;
            } else {
                nodes.push_back({TemplateNode::Kind::Escaped, tag, {}, {}});
            }
        }
        return "";
    }
};

std::string escapeHtml(const std::string& text) {
    std::string out;
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

struct Frame {
    std::string value; // raw JSON
    long long index = -1;
    std::string key;
};

class TemplateRenderer {
public:
    explicit TemplateRenderer(const std::string& json) {
        std::string root = json;
        if (root.find_first_not_of(" \t\r\n") == std::string::npos) root = "{}";
        frames.push_back({root, -1, ""});
    }

    std::string render(const std::vector<TemplateNode>& nodes) {
        std::string out;
        for (const auto& node : nodes) {
            switch (node.kind) {
                case TemplateNode::Kind::Text: out += node.text; break;
                case TemplateNode::Kind::Escaped: out += escapeHtml(text(node.text)); break;
                case TemplateNode::Kind::Raw: out += text(node.text); break;
                case TemplateNode::Kind::If: {
                    std::string value;
                    out += render(lookup(node.text, value) && truthy(value) ? node.body : node.otherwise);
                    break;
                }
                case TemplateNode::Kind::Each: out += each(node); break;
            }
        }
        return out;
    }

private:
    std::vector<Frame> frames;

    bool lookup(const std::string& path, std::string& out) const {
        const Frame& current = frames.back();
        if (path == ".") { out = current.value; return true; }
        if (path == "@index") { out = current.index < 0 ? "" : std::to_string(current.index); return current.index >= 0; }
        if (path == "@key") { out = "\"" + jsonEscape(current.key).value.str() + "\""; return !current.key.empty(); }
        for (auto frame = frames.rbegin(); frame != frames.rend(); ++frame)
            if (jsonRaw(frame->value, path, out)) return true;
        return false;
    }

    std::string text(const std::string& path) const {
        std::string raw;
        if (!lookup(path, raw) || raw == "null") return "";
        if (!raw.empty() && raw[0] == '"') return jsonGet(raw, "").value.str();
        return raw;
    }

    static bool truthy(const std::string& raw) {
        if (raw == "false" || raw == "null" || raw == "\"\"") return false;
        std::string type = jsonType(raw, "");
        if (type == "number") return jsonGet(raw, "").value.str().find_first_not_of("-0.") != std::string::npos;
        if (type == "array" || type == "object") return jsonCount(raw, "") > 0;
        return !raw.empty();
    }

    std::string each(const TemplateNode& node) {
        std::string list;
        if (!lookup(node.text, list)) return "";
        std::string out;
        long long index = 0;
        for (auto& [key, value] : jsonEntries(list)) {
            frames.push_back({value, index++, key});
            out += render(node.body);
            frames.pop_back();
        }
        return out;
    }
};

} // namespace

std::string renderTemplate(const std::string& source, const std::string& json) {
    if (!jsonValid(json.find_first_not_of(" \t\r\n") == std::string::npos ? "{}" : json))
        throw std::runtime_error("Runtime Error: template data is not valid JSON");
    auto nodes = TemplateParser(source).parse();
    return TemplateRenderer(json).render(nodes);
}

} // namespace foxlang::runtime

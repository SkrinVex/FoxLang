// Web building blocks without sockets: templates, JSON builders, URL and HTML
// coding, forms, uploads, cookies, routing, static files and error replies.
#include "foxlang/FoxLang.h"
#include "../../src/core/HttpServer.h"
#include <filesystem>
#include <fstream>
#include <iostream>

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return 1; \
        } \
    } while (0)

namespace fs = std::filesystem;
using foxlang::platform::HttpReply;

static bool throws(const std::string& code) {
    foxlang::Interpreter interp;
    return !interp.runSource(code).success;
}

static std::string header(const HttpReply& reply, const std::string& name) {
    for (const auto& [key, value] : reply.headers) if (key == name) return value;
    return "";
}

int main() {
    using namespace foxlang;
    auto step = [](const char* what) { std::cout << "-> " << what << std::endl; };

    step("templates");
    {
        std::string data = R"({"title":"Лис & <друзья>","items":[{"name":"a"},{"name":"b"}],"empty":[],"on":true,"zero":0,"map":{"x":1,"y":2}})";
        CHECK(runtime::renderTemplate("<h1>{{title}}</h1>", data) == "<h1>Лис &amp; &lt;друзья&gt;</h1>");
        CHECK(runtime::renderTemplate("{{{title}}}", data) == "Лис & <друзья>");
        CHECK(runtime::renderTemplate("{{#each items}}[{{@index}}:{{name}}:{{title}}]{{/each}}", data) ==
              "[0:a:Лис &amp; &lt;друзья&gt;][1:b:Лис &amp; &lt;друзья&gt;]");
        CHECK(runtime::renderTemplate("{{#each map}}{{@key}}={{.}};{{/each}}", data) == "x=1;y=2;");
        CHECK(runtime::renderTemplate("{{#if on}}yes{{else}}no{{/if}} {{#if zero}}yes{{else}}no{{/if}} "
                                      "{{#if empty}}yes{{else}}no{{/if}} {{#if missing}}yes{{/if}}", data) == "yes no no ");
        CHECK(runtime::renderTemplate("{{! comment }}{{missing}}.", data) == ".");
        CHECK(runtime::renderTemplate("{{#each items}}{{#if name}}<{{name}}>{{/if}}{{/each}}", data) == "<a><b>");
        CHECK(runtime::renderTemplate("plain", "") == "plain");
        CHECK(throws("string s = template_render(\"{{#each x}}\", \"{}\");"));
        CHECK(throws("string s = template_render(\"{{/if}}\", \"{}\");"));
        CHECK(throws("string s = template_render(\"x\", \"{bad\");"));
    }

    step("json builders");
    {
        CHECK(runtime::jsonSet("", "name", "\"Лис\"") == R"({"name":"Лис"})");
        CHECK(runtime::jsonSet(R"({"a":1})", "b.c", "2") == R"({"a":1,"b":{"c":2}})");
        CHECK(runtime::jsonSet(R"({"a":1,"b":{"c":2}})", "b.c", "3") == R"({"a":1,"b":{"c":3}})");
        CHECK(runtime::jsonSet("[1,2]", "2", "3") == "[1,2,3]");
        CHECK(runtime::jsonSet("[1,2]", "0", "9") == "[9,2]");
        CHECK(runtime::jsonValid(R"({"a":[1,{"b":null}]})") && !runtime::jsonValid("{\"a\":") && !runtime::jsonValid("1 2"));
        auto entries = runtime::jsonEntries(R"({"x":1,"y":"z"})");
        CHECK(entries.size() == 2 && entries[1].first == "y" && entries[1].second == "\"z\"");
        foxlang::Interpreter interp;
        auto res = interp.runSource(
            "string doc = json_set(\"\", \"name\", \"Лис \\\"1\\\"\"); doc = json_set(doc, \"tags\", [1, \"a\", true]);"
            "doc = json_set_raw(doc, \"inner\", \"{\\\"k\\\":[]}\"); string value = json_value([1.5, [2]]);"
            "bool valid = json_valid(doc);");
        CHECK(res.success);
        CHECK(interp.getGlobal("doc").text() == R"({"name":"Лис \"1\"","tags":[1,"a",true],"inner":{"k":[]}})");
        CHECK(interp.getGlobal("value").text() == "[1.5,[2]]");
        CHECK(interp.getGlobal("valid").text() == "true");
        CHECK(throws("string s = json_set_raw(\"\", \"a\", \"{oops\");"));
        CHECK(throws("string s = json_set(\"[1]\", \"5\", 1);"));
        CHECK(throws("string s = json_set(\"5\", \"a\", 1);"));
    }

    step("url and html");
    {
        CHECK(http::percentEncode("лис fox/?&=") == "%D0%BB%D0%B8%D1%81%20fox%2F%3F%26%3D");
        CHECK(http::percentDecode("%D0%BB%D0%B8%D1%81+fox%2", true) == "лис fox%2");
        CHECK(http::htmlEscape("<a href=\"x\">'&'</a>") == "&lt;a href=&quot;x&quot;&gt;&#39;&amp;&#39;&lt;/a&gt;");
        auto query = http::parseQuery("q=%D0%BB%D0%B8%D1%81&page=2&q=second&flag");
        CHECK(query["q"] == "лис" && query["page"] == "2" && query.count("flag") && query["flag"].empty());
        auto cookies = http::parseCookies("a=1; name=%D0%9B%D0%B8%D1%81; broken");
        CHECK(cookies["a"] == "1" && cookies["name"] == "Лис" && !cookies.count("broken"));
        CHECK(http::mimeType("x/Style.CSS") == "text/css; charset=utf-8" && http::mimeType("a.unknown") == "application/octet-stream");
    }

    step("forms and uploads");
    {
        platform::HttpRequest request;
        request.headers["content-type"] = "multipart/form-data; boundary=XyZ";
        request.body = "--XyZ\r\nContent-Disposition: form-data; name=\"title\"\r\n\r\nПривет\r\n"
                       "--XyZ\r\nContent-Disposition: form-data; name=\"file\"; filename=\"отчёт.txt\"\r\n"
                       "Content-Type: text/plain\r\n\r\nline1\r\nline2\r\n--XyZ--\r\n";
        auto parts = http::parseMultipart(request);
        CHECK(parts.size() == 2);
        CHECK(parts[1].filename == "отчёт.txt" && parts[1].data == "line1\r\nline2" && parts[1].contentType == "text/plain");
        CHECK(http::formValue(request, "title") == "Привет" && http::formValue(request, "file").empty());
        platform::HttpRequest urlencoded;
        urlencoded.headers["content-type"] = "application/x-www-form-urlencoded; charset=UTF-8";
        urlencoded.body = "name=%D0%9B%D0%B8%D1%81&text=a+b";
        CHECK(http::formValue(urlencoded, "name") == "Лис" && http::formValue(urlencoded, "text") == "a b");
        CHECK(http::parseMultipart(urlencoded).empty());
    }

    step("routing");
    {
        fs::path site = fs::temp_directory_path() / "foxlang-web-unit";
        fs::remove_all(site);
        fs::create_directories(site / "public" / "docs");
        std::ofstream(site / "public" / "app.js") << "console.log(1)";
        std::ofstream(site / "public" / "docs" / "index.html") << "<p>docs</p>";
        std::ofstream(site / "public" / ".env") << "SECRET=1";
        std::ofstream(site / "secret.txt") << "outside";

        foxlang::Interpreter interp;
        std::string code =
            "void user() { server_respond(200, \"user \" + request_param(\"id\") + \" \" + request_query_param(\"tab\")); }\n"
            "void me() { server_respond(200, \"me\"); }\n"
            "void files() { server_respond(200, request_param(\"rest\"), \"text/plain\"); }\n"
            "void create() { server_respond(201, request_form(\"name\")); server_header(\"X-Id\", \"7\");"
            " server_set_cookie(\"seen\", \"да\", \"Path=/\"); }\n"
            "void go() { server_redirect(\"/login\"); }\n"
            "void broken() { fail(\"boom\"); }\n"
            "void missing() { server_respond(404, \"<h1>нет</h1>\", \"text/html\"); }\n"
            "server_route(\"GET\", \"/users/:id\", \"user\"); server_route(\"GET\", \"/users/me\", \"me\");\n"
            "server_route(\"GET\", \"/files/*rest\", \"files\"); server_route(\"POST\", \"/users\", \"create\");\n"
            "server_route(\"GET\", \"/go\", \"go\"); server_route(\"GET\", \"/broken\", \"broken\");\n"
            "server_static(\"/static\", \"" + (site / "public").generic_string() + "\"); server_not_found(\"missing\");\n";
        CHECK(interp.runSource(code).success);
        auto& state = platform::serverState(interp.getContext());
        auto request = [&](const std::string& method, const std::string& path, const std::string& query = "",
                           const std::string& body = "", const std::string& type = "") {
            state.request = {};
            state.request.method = method;
            state.request.path = path;
            state.request.query = query;
            state.request.body = body;
            if (!type.empty()) state.request.headers["content-type"] = type;
            return platform::handleHttpRequest(state, interp.getContext());
        };
        auto reply = request("GET", "/users/42", "tab=%D0%B8%D0%BD%D1%84%D0%BE");
        CHECK(reply.status == 200 && reply.body == "user 42 инфо");
        CHECK(request("GET", "/users/me").body == "me"); // exact beats the pattern
        CHECK(request("GET", "/users/%D0%BB%D0%B8%D1%81").body == "user лис ");
        CHECK(request("GET", "/files/a/b/c.txt").body == "a/b/c.txt");
        CHECK(request("GET", "/files").body.empty() && request("GET", "/files").status == 200);
        reply = request("HEAD", "/users/1");
        CHECK(reply.status == 200 && reply.body == "user 1 ");
        reply = request("POST", "/users", "", "name=%D0%9B%D0%B8%D1%81", "application/x-www-form-urlencoded");
        CHECK(reply.status == 201 && reply.body == "Лис" && header(reply, "X-Id") == "7");
        CHECK(header(reply, "Set-Cookie") == "seen=%D0%B4%D0%B0; Path=/");
        reply = request("DELETE", "/users");
        CHECK(reply.status == 405 && header(reply, "Allow") == "POST");
        reply = request("GET", "/go");
        CHECK(reply.status == 302 && header(reply, "Location") == "/login");
        reply = request("GET", "/broken");
        CHECK(reply.status == 500);
        reply = request("GET", "/static/app.js");
        CHECK(reply.status == 200 && reply.body == "console.log(1)" && reply.contentType == "text/javascript; charset=utf-8");
        reply = request("GET", "/static/docs");
        CHECK(reply.status == 301 && header(reply, "Location") == "/static/docs/");
        CHECK(request("GET", "/static/docs/").body == "<p>docs</p>");
        for (const char* forbidden : {"/static/.env", "/static/../secret.txt", "/static/%2e%2e/secret.txt", "/static/docs/%2E%2E/%2E%2E/secret.txt"}) {
            reply = request("GET", forbidden);
            CHECK(reply.status == 404 && reply.body == "<h1>нет</h1>");
        }
        CHECK(interp.runSource("server_cors(\"*\");").success);
        reply = request("OPTIONS", "/anything");
        CHECK(reply.status == 204 && header(reply, "Access-Control-Allow-Origin") == "*");
        CHECK(header(request("GET", "/users/1"), "Access-Control-Allow-Methods").find("PATCH") != std::string::npos);
        CHECK(throws("server_route(\"GET\", \"/a/*rest/b\", \"x\");"));
        CHECK(throws("server_route(\"GET\", \"nope\", \"x\");"));
        CHECK(throws("server_header(\"X-A\", \"line\\nbreak\");"));
        CHECK(throws("server_redirect(\"/a\", 99);"));
        fs::remove_all(site);
    }

    std::cout << "TEST_WEB_OK" << std::endl;
    return 0;
}

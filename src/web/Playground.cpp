// The FoxLang of the browser playground: WebAssembly in a web worker. The page runs
// every program in a fresh worker and ends it when time runs out, so nothing here
// needs to stop a program; output goes to the worker's stdout and stderr.
#include "foxlang/FoxLang.h"
#include "foxlang/Lexer.h"
#include "foxlang/Parser.h"
#include "foxlang/SemanticAnalyzer.h"
#include <emscripten/emscripten.h>
#include <fstream>
#include <iostream>
#include <streambuf>
#include <string>

EM_JS_DEPS(foxlang_playground, "$UTF8ToString");

// Shows a line the program read, as a terminal would echo what was typed.
EM_JS(void, foxlang_echo_input, (const char* text, int size), {
    Module.echoInput(UTF8ToString(text, size));
});

namespace {
const char* const programPath = "/main.fox";

void save(const char* source) {
    std::ofstream(programPath, std::ios::binary | std::ios::trunc) << source;
}

// Standard input: the text of the page's input box, handed out a line at a time so
// that each line shows in the output at the moment the program reads it.
class PageInput : public std::streambuf {
public:
    explicit PageInput(std::string text) : text_(std::move(text)) {}
protected:
    int_type underflow() override {
        if (gptr() < egptr()) return traits_type::to_int_type(*gptr());
        if (at_ >= text_.size()) return traits_type::eof();
        size_t end = text_.find('\n', at_);
        end = end == std::string::npos ? text_.size() : end + 1;
        line_ = text_.substr(at_, end - at_);
        at_ = end;
        std::cout << std::flush;
        std::string shown = line_.back() == '\n' ? line_ : line_ + "\n";
        foxlang_echo_input(shown.data(), static_cast<int>(shown.size()));
        setg(line_.data(), line_.data(), line_.data() + line_.size());
        return traits_type::to_int_type(*gptr());
    }
private:
    std::string text_, line_;
    size_t at_ = 0;
};

void appendEscaped(std::string& out, const std::string& text) {
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
        else if (c == '\n') out += "\\n";
        else if (c < 0x20) { char code[8]; std::snprintf(code, sizeof code, "\\u%04x", c); out += code; }
        else out += static_cast<char>(c);
    }
}
} // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE const char* foxlang_version() {
    static const std::string version = foxlang::Interpreter::getVersion();
    return version.c_str();
}

// Runs the program with the text of the input box as its standard input; returns
// its exit code (1 after an error, which goes to stderr).
EMSCRIPTEN_KEEPALIVE int foxlang_run(const char* source, const char* input) {
    save(source);
    PageInput page(input);
    std::cin.rdbuf(&page);
    foxlang::InterpreterOptions options;
    options.loadDotEnv = false;
    foxlang::Interpreter interpreter(options);
    auto result = interpreter.runFile(programPath);
    std::cout << std::flush;
    std::cin.rdbuf(nullptr);
    if (!result.errorMessage.empty()) std::cerr << result.errorMessage << std::endl;
    return result.exitCode;
}

// The editor's problems: [{"line":1,"column":5,"severity":"error","message":"..."}].
EMSCRIPTEN_KEEPALIVE const char* foxlang_check(const char* source) {
    static std::string json;
    std::string text = source;
    foxlang::Lexer lexer(text, true);
    auto tokens = lexer.tokenize();
    std::vector<foxlang::Diagnostic> diagnostics = lexer.getDiagnostics();
    foxlang::Parser parser(std::move(tokens), programPath);
    std::vector<foxlang::Diagnostic> parsed;
    auto program = parser.parseProgramWithDiagnostics(parsed);
    diagnostics.insert(diagnostics.end(), parsed.begin(), parsed.end());
    foxlang::SemanticAnalyzer analyzer(programPath);
    analyzer.analyze(program.get());
    const auto& semantic = analyzer.getDiagnostics();
    diagnostics.insert(diagnostics.end(), semantic.begin(), semantic.end());
    json = "[";
    for (const auto& diagnostic : diagnostics) {
        if (json.size() > 1) json += ',';
        json += "{\"line\":" + std::to_string(diagnostic.range.start.line) +
                ",\"column\":" + std::to_string(diagnostic.range.start.column) +
                ",\"endLine\":" + std::to_string(diagnostic.range.end.line) +
                ",\"endColumn\":" + std::to_string(diagnostic.range.end.column) +
                ",\"severity\":\"" + diagnostic.severityString() + "\",\"message\":\"";
        appendEscaped(json, diagnostic.message);
        json += "\"}";
    }
    json += "]";
    return json.c_str();
}

} // extern "C"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "foxlang/FoxLang.h"
#include "foxlang/SemanticAnalyzer.h"
#include "foxlang/Project.h"
#include <filesystem>
#include "Image.h"
#include "DebugAdapter.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

bool foxlangHasBundleDescriptor();

// Reports the diagnostics the editor already sees, for a terminal or CI, without executing code.
static int check(const std::string& path) {
    std::ifstream file(foxlang::platform::pathFromUtf8(path), std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("could not open file '" + path + "'");
    std::stringstream buffer;
    buffer << file.rdbuf();

    foxlang::Lexer lexer(buffer.str(), true);
    auto tokens = lexer.tokenize();
    std::vector<foxlang::Diagnostic> diagnostics = lexer.getDiagnostics();

    foxlang::Parser parser(std::move(tokens), path);
    std::vector<foxlang::Diagnostic> parserDiagnostics;
    auto program = parser.parseProgramWithDiagnostics(parserDiagnostics);
    diagnostics.insert(diagnostics.end(), parserDiagnostics.begin(), parserDiagnostics.end());

    // Other files of the same program declare what this one may use: the project is
    // searched below the working directory when the file is inside it.
    std::string home = foxlang::platform::getEnvVar("FOXLANG_HOME");
    auto sources = std::make_shared<foxlang::OverlaySources>(foxlang::filesystemSources(home));
    foxlang::ProjectIndex project(sources);
    std::string canonical = foxlang::canonicalPath(path);
    std::string cwd = foxlang::platform::pathToUtf8(std::filesystem::current_path());
    if (canonical.rfind(cwd, 0) == 0) project.setRoot(cwd);
    foxlang::SemanticAnalyzer analyzer(canonical, home, sources);
    analyzer.addProjectFiles(project.peers(canonical));
    analyzer.analyze(program.get());
    const auto& semantic = analyzer.getDiagnostics();
    diagnostics.insert(diagnostics.end(), semantic.begin(), semantic.end());

    int errors = 0;
    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.severity == foxlang::DiagnosticSeverity::Error) ++errors;
        std::cout << path << ":" << diagnostic.range.start.line << ":" << diagnostic.range.start.column
                  << ": " << diagnostic.severityString() << ": " << diagnostic.message << std::endl;
    }
    if (diagnostics.empty()) std::cout << path << ": no problems found" << std::endl;
    return errors ? 1 : 0;
}

static int report(const foxlang::RunResult& result) {
    if (!result.errorMessage.empty()) std::cerr << "FoxLang: " << result.errorMessage << std::endl;
    return result.exitCode;
}

// Program arguments as UTF-8. Windows hands main() its ANSI code page, which cannot
// hold Cyrillic, so the wide command line is converted instead.
static std::vector<std::string> argumentsFrom(int argc, char* argv[], int first) {
    std::vector<std::string> arguments;
#ifdef _WIN32
    int count = 0;
    LPWSTR* wide = CommandLineToArgvW(GetCommandLineW(), &count);
    if (wide && count == argc) {
        for (int i = first; i < count; ++i) {
            int size = WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, nullptr, 0, nullptr, nullptr);
            std::string text(size > 0 ? static_cast<size_t>(size - 1) : 0, '\0');
            if (size > 1) WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, text.data(), size, nullptr, nullptr);
            arguments.push_back(std::move(text));
        }
        LocalFree(wide);
        return arguments;
    }
    if (wide) LocalFree(wide);
#endif
    for (int i = first; i < argc; ++i) arguments.emplace_back(argv[i]);
    return arguments;
}

int main(int argc, char* argv[]) {
    try {
    const std::vector<std::string> args = argumentsFrom(argc, argv, 0);
    if (argc == 2 && std::string(argv[1]) == "--foxlang-licenses") {
        std::cout << foxlang::platform::thirdPartyLicenses();
        return 0;
    }
#if (defined(__linux__) || defined(_WIN32)) && (defined(__x86_64__) || defined(_M_X64))
    if (!foxlangHasBundleDescriptor()) throw std::runtime_error("Bundle Error: damaged stub descriptor");
    auto embedded = foxlang::bundle::unpack(foxlang::bundle::readImage(foxlang::bundle::executablePath()));
    if (embedded) {
        auto sources = std::make_shared<foxlang::bundle::Sources>(std::move(*embedded));
        foxlang::InterpreterOptions options;
        options.sources = sources;
        options.loadDotEnv = false;
        options.arguments = argumentsFrom(argc, argv, 1);
        foxlang::Interpreter interpreter(options);
        return report(interpreter.runSource(sources->read(sources->entry), sources->entry));
    }
#endif
    const std::string version = foxlang::Interpreter::getVersion();

    if (argc == 2 && (std::string(argv[1]) == "--version" || std::string(argv[1]) == "-v")) {
        std::cout << "FoxLang " << version << std::endl;
        return 0;
    }

    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        std::cout << "FoxLang " << version << "\n\n"
                  << "Usage:\n"
                  << "  foxlang <script.fox> [args...]  Run a FoxLang program; args reach os_args()\n"
                  << "  foxlang check <script.fox>      Report problems without running the program\n"
                  << "  foxlang build <file.fox> [-o|--output app]\n"
                  << "                                  Bundle a standalone executable for this OS/architecture\n"
                  << "                                  Includes runtime and source modules; no compiler needed\n"
                  << "  foxlang debug-adapter [--connect host:port]\n"
                  << "                                  Debug Adapter Protocol server for editors (VS Code, Kate, Zed)\n"
                  << "  foxlang --version               Show version\n"
                  << "  foxlang --help                  Show this help\n"
                  << "  foxlang --foxlang-licenses      Show embedded dependency licenses\n\n"
                  << "Environment:\n"
                  << "  FOXLANG_HOME            FoxLang installation/std library path\n"
                  << "  FOXLANG_LOG_LEVEL       debug | info | warn | error | off\n"
                  << "  FOXLANG_CA_BUNDLE       PEM CA file | embedded (default) | system\n\n"
                  << "Standalone: .env/resources are not bundled; HTTP(S) and TCP are built in.\n\n"
                  << "Repository & documentation:\n"
                  << "  https://github.com/SkrinVex/FoxLang\n"
                  << "  https://github.com/SkrinVex/FoxLang/blob/master/DOCUMENTATION.md\n";
        return 0;
    }

    if (argc < 2) {
        std::cout << "FoxLang " << version << "\nUsage: foxlang <script.fox> [args...]\n"
                  << "       foxlang --help\n"
                  << "       foxlang --version" << std::endl;
        return 1;
    }

    if (std::string(argv[1]) == "check") {
        if (argc != 3) throw std::runtime_error("Usage: foxlang check <script.fox>");
        return check(args[2]);
    }

    if (std::string(argv[1]) == "debug-adapter") {
        return foxlang::debug::runAdapter(std::vector<std::string>(args.begin() + 2, args.end()));
    }

    if (std::string(argv[1]) == "build") {
        if (argc != 3 && argc != 5) {
            throw std::runtime_error("Usage: foxlang build <file.fox> [-o|--output app]");
        }
        if (args[2].empty() || args[2][0] == '-') throw std::runtime_error("Build Error: expected an input source file");
        std::string output;
        if (argc == 5) {
            if (std::string(argv[3]) != "-o" && std::string(argv[3]) != "--output") throw std::runtime_error("Build Error: unknown option '" + std::string(argv[3]) + "'");
            output = args[4];
            if (output.empty() || output[0] == '-') throw std::runtime_error("Build Error: expected output filename");
        }
        foxlang::bundle::build(foxlang::platform::pathFromUtf8(args[2]), foxlang::platform::pathFromUtf8(output));
        return 0;
    }

    foxlang::InterpreterOptions options;
    options.arguments = argumentsFrom(argc, argv, 2);
    foxlang::Interpreter interpreter(options);
    return report(interpreter.runFile(args[1]));
    } catch (const std::exception& error) {
        std::cerr << "FoxLang: " << error.what() << std::endl;
        return 1;
    }
}

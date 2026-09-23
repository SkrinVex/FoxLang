#include <iostream>
#include <string>
#include "foxlang/FoxLang.h"
#include "Image.h"

bool foxlangHasBundleDescriptor();

static int report(const foxlang::RunResult& result) {
    if (!result.success) std::cerr << "FoxLang: " << result.errorMessage << std::endl;
    return result.exitCode;
}

int main(int argc, char* argv[]) {
    try {
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
                  << "  foxlang <script.fox>    Run a FoxLang program\n"
                  << "  foxlang build <file.fox> [-o|--output app]\n"
                  << "                         Bundle a standalone executable for this OS/architecture\n"
                  << "                         Includes runtime and source modules; no compiler needed\n"
                  << "  foxlang --version       Show version\n"
                  << "  foxlang --help          Show this help\n\n"
                  << "  foxlang --foxlang-licenses  Show embedded dependency licenses\n\n"
                  << "Environment:\n"
                  << "  FOXLANG_HOME            FoxLang installation/std library path\n"
                  << "  FOXLANG_LOG_LEVEL       debug | info | warn | error | off\n"
                  << "  FOXLANG_LOG             Legacy master log switch\n\n"
                  << "  FOXLANG_CA_BUNDLE       PEM CA file for HTTPS (default: OS trust store)\n\n"
                  << "Standalone: .env/resources are not bundled; HTTP(S) and TCP are built in.\n\n"
                  << "Repository & documentation:\n"
                  << "  https://github.com/SkrinVex/FoxLang\n"
                  << "  https://github.com/SkrinVex/FoxLang/blob/master/DOCUMENTATION.md\n";
        return 0;
    }

    if (argc < 2) {
        std::cout << "FoxLang " << version << "\nUsage: foxlang <script.fox>\n"
                  << "       foxlang --help\n"
                  << "       foxlang --version" << std::endl;
        return 1;
    }

    if (std::string(argv[1]) == "build") {
        if (argc != 3 && argc != 5) {
            throw std::runtime_error("Usage: foxlang build <file.fox> [-o|--output app]");
        }
        if (std::string(argv[2]).empty() || argv[2][0] == '-') throw std::runtime_error("Build Error: expected an input source file");
        std::string output;
        if (argc == 5) {
            if (std::string(argv[3]) != "-o" && std::string(argv[3]) != "--output") throw std::runtime_error("Build Error: unknown option '" + std::string(argv[3]) + "'");
            output = argv[4];
            if (output.empty() || output[0] == '-') throw std::runtime_error("Build Error: expected output filename");
        }
        foxlang::bundle::build(argv[2], output);
        return 0;
    }

    foxlang::Interpreter interpreter;
    return report(interpreter.runFile(argv[1]));
    } catch (const std::exception& error) {
        std::cerr << "FoxLang: " << error.what() << std::endl;
        return 1;
    }
}

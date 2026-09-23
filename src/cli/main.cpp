#include <iostream>
#include <string>
#include "foxlang/FoxLang.h"

int main(int argc, char* argv[]) {
    const std::string version = foxlang::Interpreter::getVersion();

    if (argc == 2 && (std::string(argv[1]) == "--version" || std::string(argv[1]) == "-v")) {
        std::cout << "FoxLang " << version << std::endl;
        return 0;
    }

    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        std::cout << "FoxLang " << version << "\n\n"
                  << "Usage:\n"
                  << "  foxlang <script.fox>    Run a FoxLang program\n"
                  << "  foxlang --version       Show version\n"
                  << "  foxlang --help          Show this help\n\n"
                  << "Environment:\n"
                  << "  FOXLANG_HOME            FoxLang installation/std library path\n"
                  << "  FOXLANG_LOG_LEVEL       debug | info | warn | error | off\n"
                  << "  FOXLANG_LOG             Legacy master log switch\n\n"
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

    foxlang::Interpreter interpreter;
    foxlang::RunResult result = interpreter.runFile(argv[1]);

    if (!result.success) {
        std::cerr << "FoxLang: " << result.errorMessage << std::endl;
        return result.exitCode;
    }

    return 0;
}

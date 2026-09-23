#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include "Lexer.h"
#include "Parser.h"

static constexpr const char* FOX_VERSION = "5.4.3";

static void loadDotEnv(const std::string& scriptPath) {
    std::string dir = ".";
    size_t slash = scriptPath.find_last_of("/\\");
    if (slash != std::string::npos) dir = scriptPath.substr(0, slash);
    std::ifstream env(dir + "/.env");
    if (!env.is_open()) env.open(".env");
    std::string line;
    while (std::getline(env, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line.rfind("export ", 0) == 0) line = line.substr(7);
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        size_t ks = key.find_first_not_of(" \t"); if (ks != std::string::npos) key = key.substr(ks);
        if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\''))) value = value.substr(1, value.size()-2);
        if (!key.empty() && std::getenv(key.c_str()) == nullptr) {
#ifdef _WIN32
            _putenv_s(key.c_str(), value.c_str());
#else
            setenv(key.c_str(), value.c_str(), 0);
#endif
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc == 2 && (std::string(argv[1]) == "--version" || std::string(argv[1]) == "-v")) {
        std::cout << "FoxLang " << FOX_VERSION << std::endl;
        return 0;
    }

    if (argc < 2) {
        std::cout << "FoxLang " << FOX_VERSION << "\nUsage: foxlang <script.fox>\n"
                  << "       foxlang --version" << std::endl;
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "FoxLang: could not open file '" << argv[1] << "'" << std::endl;
        return 1;
    }

    try {
        loadDotEnv(argv[1]);
        std::stringstream buffer;
        buffer << file.rdbuf();

        Lexer lexer(buffer.str());
        Parser parser(lexer.tokenize());
        parser.currentFile = argv[1];
        parser.run();
    } catch (const std::exception& e) {
        std::cerr << "FoxLang: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

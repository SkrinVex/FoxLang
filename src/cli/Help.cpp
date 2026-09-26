#include "Help.h"
#include <map>

namespace foxlang::cli {
namespace {

// What `foxlang help <command>` and `foxlang <command> --help` show.
const std::map<std::string, const char*>& commands() {
    static const std::map<std::string, const char*> text = {
        {"run",
         "Usage: foxlang <script.fox> [args...]\n"
         "\n"
         "Runs a program. Everything after the file name reaches the program through\n"
         "os_args(). A .env file next to the script (or in the current directory) sets\n"
         "environment variables that are not set already.\n"
         "\n"
         "Examples:\n"
         "  foxlang hello.fox\n"
         "  foxlang tool.fox input.txt --verbose\n"},
        {"check",
         "Usage: foxlang check <script.fox>\n"
         "\n"
         "Reports syntax errors, type errors and unknown names without running the\n"
         "program. Exits with 1 when there is an error.\n"
         "\n"
         "Example:\n"
         "  foxlang check app.fox\n"},
        {"build",
         "Usage: foxlang build <file.fox> [-o|--output name]\n"
         "\n"
         "Builds one standalone executable for this OS and CPU: the interpreter, the\n"
         "program and every module it uses (using, include) in a single file. The people\n"
         "who run it need neither FoxLang nor a compiler.\n"
         "\n"
         "  -o, --output name   Output file; the script's name by default. On Windows\n"
         "                      .exe is added. An existing file is never overwritten.\n"
         "\n"
         "Not inside the file: .env, environment variables and resources the program\n"
         "reads with read_file; pass secrets through the environment at run time. A Linux\n"
         "build makes a Linux program, a Windows build an .exe; there is no cross-building.\n"
         "The sources are stored inside and can be extracted: this is not code protection.\n"
         "\n"
         "Examples:\n"
         "  foxlang build game.fox              makes ./game (game.exe on Windows)\n"
         "  foxlang build server.fox -o api     makes ./api\n"},
        {"test",
         "Usage: foxlang test [--filter text] [files or directories...]\n"
         "\n"
         "Runs every parameterless test_ function of the *_test.fox files found (in the\n"
         "current directory by default), each in a fresh program. Exits with 1 when a\n"
         "test fails.\n"
         "\n"
         "  --filter text   Only tests whose name contains the text\n"
         "\n"
         "Examples:\n"
         "  foxlang test\n"
         "  foxlang test tests --filter parse\n"},
        {"fmt",
         "Usage: foxlang fmt [--check] [files or directories...]\n"
         "\n"
         "Lays out .fox files: indentation, blank lines and spaces (the current directory\n"
         "by default).\n"
         "\n"
         "  --check   Change nothing; list the files that would change and exit with 1\n"
         "            when there are any (for CI)\n"
         "\n"
         "Examples:\n"
         "  foxlang fmt\n"
         "  foxlang fmt --check src\n"},
        {"disasm",
         "Usage: foxlang disasm <script.fox>\n"
         "\n"
         "Prints the bytecode the program and each of its functions run as: source line,\n"
         "instruction number, instruction and its registers.\n"},
        {"debug-adapter",
         "Usage: foxlang debug-adapter [--connect host:port]\n"
         "\n"
         "Debug Adapter Protocol server for editors (VS Code, Kate, Zed): breakpoints,\n"
         "stepping, variables. Talks over stdin/stdout; with --connect it connects to\n"
         "the editor over TCP and the program keeps its own terminal.\n"},
    };
    return text;
}

} // namespace

void printHelp(std::ostream& out, const std::string& version) {
    out << "FoxLang " << version << " - scripts, console tools, servers and 2D games\n"
        << "\n"
        << "Usage: foxlang <script.fox> [args...]\n"
        << "       foxlang <command> [options]\n"
        << "\n"
        << "Run and check:\n"
        << "  foxlang <script.fox> [args...]     Run a program; args reach os_args()\n"
        << "  foxlang check <script.fox>         Find errors without running the program\n"
        << "  foxlang test [--filter text] [paths...]\n"
        << "                                     Run the test_ functions of *_test.fox files\n"
        << "\n"
        << "Build:\n"
        << "  foxlang build <file.fox> [-o name] One standalone executable for this OS and CPU:\n"
        << "                                     runtime, program and modules in one file\n"
        << "\n"
        << "Tools:\n"
        << "  foxlang fmt [--check] [paths...]   Lay out .fox files\n"
        << "  foxlang disasm <script.fox>        Print the bytecode the program runs as\n"
        << "  foxlang debug-adapter [--connect host:port]\n"
        << "                                     Debugger for editors (VS Code, Kate, Zed)\n"
        << "\n"
        << "Information:\n"
        << "  foxlang help [command]             This help, or the details of one command\n"
        << "  foxlang --version                  Version\n"
        << "  foxlang --foxlang-licenses         Licenses of the built-in libraries\n"
        << "\n"
        << "Examples:\n"
        << "  foxlang hello.fox\n"
        << "  foxlang build game.fox             makes ./game (game.exe on Windows)\n"
        << "  foxlang help build\n"
        << "\n"
        << "Environment:\n"
        << "  FOXLANG_HOME        Installation and standard library path\n"
        << "  FOXLANG_LOG_LEVEL   debug | info | warn | error | off\n"
        << "  FOXLANG_CA_BUNDLE   PEM CA file | embedded (default) | system\n"
        << "  FOXLANG_X11_SHM     0 turns off shared-memory frames on X11\n"
        << "\n"
        << "Documentation:\n"
        << "  https://github.com/SkrinVex/FoxLang/blob/master/DOCUMENTATION.md\n";
}

bool printCommandHelp(std::ostream& out, const std::string& command) {
    auto found = commands().find(command);
    if (found == commands().end()) return false;
    out << found->second;
    return true;
}

std::string commandNames() {
    std::string names;
    for (const auto& [name, text] : commands()) names += (names.empty() ? "" : ", ") + name;
    return names;
}

} // namespace foxlang::cli

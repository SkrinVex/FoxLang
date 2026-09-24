#include "foxlang/Platform.h"
#include <iostream>
#include <cstdlib>

#ifdef _WIN32
    #include <conio.h>
    #include <cstdio>
    #include <windows.h>
#else
    #include <termios.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/select.h>
#endif

namespace foxlang {
namespace platform {

std::string getch() {
#ifdef _WIN32
    return std::string(1, static_cast<char>(_getch()));
#else
    struct termios oldt, newt;
    if (tcgetattr(STDIN_FILENO, &oldt) != 0) return "";
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    int ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    if (ch == EOF) return "";
    return std::string(1, static_cast<char>(ch));
#endif
}

bool kbhit() {
#ifdef _WIN32
    return _kbhit() != 0;
#else
    struct termios oldt, newt;
    if (tcgetattr(STDIN_FILENO, &oldt) != 0) return false;
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    timeval tv{0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    int ready = select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ready > 0;
#endif
}

bool setEnvVar(const std::string& key, const std::string& value) {
#ifdef _WIN32
    return _putenv_s(key.c_str(), value.c_str()) == 0;
#else
    return setenv(key.c_str(), value.c_str(), 1) == 0;
#endif
}

std::string getEnvVar(const std::string& key) {
    const char* val = std::getenv(key.c_str());
    return val ? std::string(val) : "";
}

} // namespace platform
} // namespace foxlang

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
    #include <pthread.h>
#endif

namespace foxlang {
namespace platform {

// How much stack this thread actually has decides how deep a program may recurse:
// Windows reserves 1 MB per thread by default where Linux gives 8, and one FoxLang
// call costs a different number of kilobytes per compiler. Three fifths leaves room
// for the unwinding and for the error report itself.
size_t stackBudget() {
    size_t size = 0;
#ifdef _WIN32
    ULONG_PTR low = 0, high = 0;
    GetCurrentThreadStackLimits(&low, &high);
    if (high > low) size = static_cast<size_t>(high - low);
#else
    pthread_attr_t attributes;
    if (pthread_getattr_np(pthread_self(), &attributes) == 0) {
        void* address = nullptr;
        size_t reported = 0;
        if (pthread_attr_getstack(&attributes, &address, &reported) == 0) size = reported;
        pthread_attr_destroy(&attributes);
    }
#endif
    if (size < (1u << 20)) size = 1u << 20; // A stack we cannot measure is assumed small.
    return size / 5 * 3;
}

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

// Starting other programs: no shell is involved, so arguments reach the program
// exactly as given and cannot be reinterpreted as commands.
#include "foxlang/Platform.h"
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
extern char** environ;
#endif

namespace foxlang::platform {
namespace {
constexpr std::size_t maxOutput = 16 * 1024 * 1024;

#ifdef _WIN32
std::wstring wide(const std::string& text) {
    if (text.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size);
    return out;
}

// Quoting that CommandLineToArgvW and the C runtime undo exactly.
void appendQuoted(std::wstring& line, const std::wstring& arg) {
    if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        line += arg;
        return;
    }
    line += L'"';
    for (size_t i = 0;; ++i) {
        size_t backslashes = 0;
        while (i < arg.size() && arg[i] == L'\\') { ++i; ++backslashes; }
        if (i == arg.size()) {
            line.append(backslashes * 2, L'\\');
            break;
        }
        if (arg[i] == L'"') line.append(backslashes * 2 + 1, L'\\');
        else line.append(backslashes, L'\\');
        line += arg[i];
    }
    line += L'"';
}
#endif
} // namespace

int runProcess(const std::string& program, const std::vector<std::string>& args, std::string* output) {
    if (program.empty() || program.find('\0') != std::string::npos)
        throw std::runtime_error("Runtime Error: program name is empty or invalid");
#ifdef _WIN32
    std::wstring line;
    appendQuoted(line, wide(program));
    for (const auto& arg : args) {
        line += L' ';
        appendQuoted(line, wide(arg));
    }
    SECURITY_ATTRIBUTES inherit{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE readPipe = nullptr, writePipe = nullptr;
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    if (output) {
        if (!CreatePipe(&readPipe, &writePipe, &inherit, 0)) return -1;
        SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdOutput = writePipe;
        startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    }
    PROCESS_INFORMATION process{};
    BOOL started = CreateProcessW(nullptr, line.data(), nullptr, nullptr, output ? TRUE : FALSE, 0, nullptr, nullptr,
                                  &startup, &process);
    if (writePipe) CloseHandle(writePipe);
    if (!started) {
        if (readPipe) CloseHandle(readPipe);
        return -1;
    }
    if (output) {
        char buffer[4096];
        DWORD count = 0;
        while (ReadFile(readPipe, buffer, sizeof(buffer), &count, nullptr) && count > 0)
            if (output->size() < maxOutput) output->append(buffer, count);
        CloseHandle(readPipe);
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return static_cast<int>(code);
#else
    std::vector<std::string> storage{program};
    storage.insert(storage.end(), args.begin(), args.end());
    std::vector<char*> argv;
    for (auto& arg : storage) argv.push_back(arg.data());
    argv.push_back(nullptr);
    int pipeFds[2] = {-1, -1};
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    if (output) {
        if (pipe(pipeFds) != 0) { posix_spawn_file_actions_destroy(&actions); return -1; }
        posix_spawn_file_actions_adddup2(&actions, pipeFds[1], STDOUT_FILENO);
        posix_spawn_file_actions_addclose(&actions, pipeFds[0]);
        posix_spawn_file_actions_addclose(&actions, pipeFds[1]);
    }
    pid_t pid = 0;
    int error = posix_spawnp(&pid, program.c_str(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    if (output) close(pipeFds[1]);
    if (error != 0) {
        if (output) close(pipeFds[0]);
        return -1;
    }
    if (output) {
        char buffer[4096];
        for (;;) {
            ssize_t count = read(pipeFds[0], buffer, sizeof(buffer));
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) break;
            if (output->size() < maxOutput) output->append(buffer, static_cast<size_t>(count));
        }
        close(pipeFds[0]);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return 128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
#endif
}

bool openWithDefaultApp(const std::string& target) {
    if (target.empty() || target.find('\0') != std::string::npos) return false;
#ifdef _WIN32
    auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", wide(target).c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    return result > 32;
#elif defined(__APPLE__)
    return runProcess("open", {target}, nullptr) == 0;
#else
    return runProcess("xdg-open", {target}, nullptr) == 0;
#endif
}

} // namespace foxlang::platform

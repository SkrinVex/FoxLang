// Public, interpreter-owned child processes. No shell and no VM calls from threads.
#include "Builtin.h"
#include "foxlang/Platform.h"
#include <algorithm>
#include <chrono>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif
#endif

namespace foxlang::platform {
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
namespace {
constexpr size_t captureLimit = 16 * 1024 * 1024;
[[noreturn]] void processError(const char* operation, int code) {
    throw std::runtime_error(std::string("Process Error: ") + operation + " (" + std::to_string(code) + ")");
}
#ifdef _WIN32
struct Handle {
    HANDLE value = nullptr;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
std::wstring quoted(const std::string& utf8) {
    auto arg = pathFromUtf8(utf8).native();
    std::wstring result = L"\"";
    size_t slashes = 0;
    for (wchar_t ch : arg) {
        if (ch == L'\\') { ++slashes; continue; }
        result.append(slashes * (ch == L'"' ? 2 : 1), L'\\');
        slashes = 0;
        if (ch == L'"') result += L'\\';
        result += ch;
    }
    result.append(slashes * 2, L'\\');
    return result + L'"';
}
#else
struct Descriptor {
    int value = -1;
    ~Descriptor() { if (value >= 0) ::close(value); }
    void close() { if (value >= 0) ::close(value); value = -1; }
};
#endif
struct Child {
    std::chrono::steady_clock::time_point deadline;
    std::string output, errors;
    bool done = false, timedOut = false, cancelled = false, truncated = false;
    int code = -1;
#ifdef _WIN32
    Handle process, job, outRead, errRead;
#else
    pid_t pid = -1;
    Descriptor outRead, errRead;
#endif
    void stop() noexcept {
#ifdef _WIN32
        if (job.value) TerminateJobObject(job.value, 125);
#else
        if (pid > 0) kill(-pid, SIGKILL);
#endif
    }
    ~Child() {
        stop();
#ifdef _WIN32
        if (process.value) WaitForSingleObject(process.value, INFINITE);
#else
        if (pid > 0 && !done) { int status; while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {} }
#endif
    }
    void append(std::string& target, const char* data, size_t count) {
        size_t available = captureLimit - target.size();
        target.append(data, std::min(available, count));
        truncated = truncated || count > available;
    }
    void drain() {
        // Bound work per poll so a continuously writing child cannot starve the UI.
        for (int stream = 0; stream < 2; ++stream) {
            auto& pipe = stream ? errRead : outRead;
            auto& target = stream ? errors : output;
            char buffer[8192];
            for (int chunk = 0; chunk < 128; ++chunk) {
#ifdef _WIN32
                DWORD available = 0, count = 0;
                if (!PeekNamedPipe(pipe.value, nullptr, 0, nullptr, &available, nullptr)) {
                    if (GetLastError() == ERROR_BROKEN_PIPE) break;
                    processError("inspect output pipe", static_cast<int>(GetLastError()));
                }
                if (!available) break;
                if (!ReadFile(pipe.value, buffer, std::min<DWORD>(available, sizeof(buffer)), &count, nullptr))
                    processError("read output pipe", static_cast<int>(GetLastError()));
#else
                auto count = ::read(pipe.value, buffer, sizeof(buffer));
                if (count < 0 && errno == EINTR) continue;
                if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
                if (count < 0) processError("read output pipe", errno);
#endif
                if (!count) break;
                append(target, buffer, static_cast<size_t>(count));
            }
        }
    }
    bool poll() {
        if (done) return true;
        drain();
#ifdef _WIN32
        DWORD status = WaitForSingleObject(process.value, 0);
        if (status == WAIT_FAILED) processError("wait", static_cast<int>(GetLastError()));
        if (status == WAIT_OBJECT_0) {
            DWORD exitCode;
            if (!GetExitCodeProcess(process.value, &exitCode)) processError("exit code", static_cast<int>(GetLastError()));
            code = static_cast<int>(exitCode); done = true;
        }
#else
        int status = 0;
        pid_t result = waitpid(pid, &status, WNOHANG);
        if (result < 0 && errno != EINTR) processError("wait", errno);
        if (result == pid) { code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status); done = true; }
#endif
        if (done) {
            stop(); drain();
#ifndef _WIN32
            pid = -1; // Never signal a potentially reused PID after this group was reaped.
#endif
        }
        else if (std::chrono::steady_clock::now() >= deadline) { timedOut = !cancelled; stop(); }
        return done;
    }
};
std::unique_ptr<Child> start(const std::string& program, const std::vector<std::string>& args, int timeout) {
    if (program.empty() || program.find('\0') != std::string::npos) processError("invalid program", 0);
    for (const auto& arg : args) if (arg.find('\0') != std::string::npos) processError("NUL argument", 0);
    auto child = std::make_unique<Child>();
    child->deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
#ifdef _WIN32
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    Handle outWrite, errWrite, input;
    if (!CreatePipe(&child->outRead.value, &outWrite.value, &attributes, 0) ||
        !CreatePipe(&child->errRead.value, &errWrite.value, &attributes, 0) ||
        !SetHandleInformation(child->outRead.value, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(child->errRead.value, HANDLE_FLAG_INHERIT, 0))
        processError("create pipes", static_cast<int>(GetLastError()));
    input.value = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &attributes, OPEN_EXISTING, 0, nullptr);
    if (input.value == INVALID_HANDLE_VALUE) processError("open stdin", static_cast<int>(GetLastError()));
    child->job.value = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!child->job.value || !SetInformationJobObject(child->job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
        processError("create process job", static_cast<int>(GetLastError()));
    std::wstring command = quoted(program);
    for (const auto& arg : args) command += L" " + quoted(arg);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input.value; startup.hStdOutput = outWrite.value; startup.hStdError = errWrite.value;
    PROCESS_INFORMATION info{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_SUSPENDED | CREATE_NO_WINDOW,
                        nullptr, nullptr, &startup, &info)) processError("start", static_cast<int>(GetLastError()));
    child->process.value = info.hProcess;
    Handle thread; thread.value = info.hThread;
    if (!AssignProcessToJobObject(child->job.value, info.hProcess)) {
        DWORD error = GetLastError(); TerminateProcess(info.hProcess, 125);
        processError("assign process job", static_cast<int>(error));
    }
    if (ResumeThread(info.hThread) == static_cast<DWORD>(-1)) processError("resume process", static_cast<int>(GetLastError()));
#else
    Descriptor outWrite, errWrite;
    auto pipePair = [](Descriptor& readEnd, Descriptor& writeEnd) {
        int pair[2]; if (::pipe(pair) != 0) processError("pipe", errno);
        readEnd.value = pair[0]; writeEnd.value = pair[1];
        if (fcntl(pair[0], F_SETFD, FD_CLOEXEC) < 0 || fcntl(pair[1], F_SETFD, FD_CLOEXEC) < 0 ||
            fcntl(pair[0], F_SETFL, O_NONBLOCK) < 0) processError("pipe flags", errno);
    };
    pipePair(child->outRead, outWrite); pipePair(child->errRead, errWrite);
    posix_spawn_file_actions_t actions;
    int error = posix_spawn_file_actions_init(&actions);
    if (error) processError("spawn actions", error);
    struct ActionsGuard { posix_spawn_file_actions_t* p; ~ActionsGuard() { posix_spawn_file_actions_destroy(p); } } guard{&actions};
    auto check = [](int value) { if (value) processError("spawn setup", value); };
    check(posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0));
    check(posix_spawn_file_actions_adddup2(&actions, outWrite.value, STDOUT_FILENO));
    check(posix_spawn_file_actions_adddup2(&actions, errWrite.value, STDERR_FILENO));
    posix_spawnattr_t attr;
    check(posix_spawnattr_init(&attr));
    struct AttrGuard { posix_spawnattr_t* p; ~AttrGuard() { posix_spawnattr_destroy(p); } } attrGuard{&attr};
    check(posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP));
    check(posix_spawnattr_setpgroup(&attr, 0));
    std::vector<std::string> storage{program}; storage.insert(storage.end(), args.begin(), args.end());
    std::vector<char*> argv;
    for (auto& arg : storage) argv.push_back(arg.data());
    argv.push_back(nullptr);
    error = posix_spawnp(&child->pid, program.c_str(), &actions, &attr, argv.data(), environ);
    if (error) { child->pid = -1; processError("start", error); }
#endif
    return child;
}
} // namespace
struct ProcessTable { int next = 1; std::map<int, std::unique_ptr<Child>> children; };
#else
struct ProcessTable {};
#endif
} // namespace foxlang::platform

namespace foxlang::runtime {
namespace {
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
platform::ProcessTable& table(Call& c) {
    auto& slot = c.ctx.getRoot()->processes;
    if (!slot) slot = std::make_shared<platform::ProcessTable>();
    return *slot;
}
platform::Child& child(Call& c) {
    auto& entries = table(c).children;
    auto it = entries.find(static_cast<int>(c.integer(0)));
    if (it == entries.end()) throw std::runtime_error("Process Error: unknown process handle");
    return *it->second;
}
Value operation(Call& c) {
    if (c.spec.name == "process_start") {
        auto& owner = table(c);
        if (owner.children.size() >= 32 || owner.next == std::numeric_limits<int>::max())
            throw std::runtime_error("Process Error: process handle limit; release completed handles");
        int timeout = static_cast<int>(c.amount(2, 3600000));
        if (!timeout) throw std::runtime_error("Process Error: timeout must be 1..3600000 ms");
        std::vector<std::string> args;
        for (const auto& item : c.array(1)) {
            if (!item.isString()) throw std::runtime_error("Process Error: arguments must be strings");
            args.push_back(item.str());
        }
        auto value = platform::start(c.text(0), args, timeout);
        int id = owner.next++;
        owner.children.emplace(id, std::move(value));
        return integer(id);
    }
    auto& value = child(c);
    if (c.spec.name == "process_poll") return boolean(value.poll());
    if (c.spec.name == "process_cancel") { if (!value.poll()) { value.cancelled = true; value.stop(); } return nothing(); }
    if (c.spec.name == "process_release") {
        if (!value.poll()) throw std::runtime_error("Process Error: cancel and await completion before release");
        table(c).children.erase(static_cast<int>(c.integer(0))); return nothing();
    }
    if (!value.poll()) throw std::runtime_error("Process Error: result is not ready");
    return makeArray({integer(value.code), text(value.output), text(value.errors), boolean(value.timedOut),
                      boolean(value.cancelled), boolean(value.truncated)});
}
#else
Value operation(Call&) { throw std::runtime_error("Process Error: child processes are unavailable on this platform"); }
#endif
}
void addProcessBuiltins(std::vector<Builtin>& out) {
    out.push_back({{"process_start", "int", {{"string", "program"}, {"array", "args"}, {"int", "timeout_ms"}}, 3, false, "os",
        "Запускает процесс без shell; stdin закрыт. Возвращает handle. Требуется регулярный process_poll; тайм-аут 1..3600000 мс. Ошибки запуска — Process Error. Недоступно в браузере/Android."}, operation});
    out.push_back({{"process_poll", "bool", {{"int", "handle"}}, 1, false, "os", "Неблокирующий сбор stdout/stderr и проверка завершения/тайм-аута. true после завершения."}, operation});
    out.push_back({{"process_result", "array", {{"int", "handle"}}, 1, false, "os", "После завершения: [exit_code, stdout, stderr, timed_out, cancelled, truncated]. Вывод ограничен 16 МиБ для каждого потока. До завершения — Process Error."}, operation});
    out.push_back({{"process_cancel", "void", {{"int", "handle"}}, 1, false, "os", "Запрашивает принудительное завершение группы процесса. Затем дождитесь process_poll и освободите handle."}, operation});
    out.push_back({{"process_release", "void", {{"int", "handle"}}, 1, false, "os", "Освобождает завершённый процесс. Неизвестный/работающий handle — Process Error. При уничтожении runtime оставшиеся процессы останавливаются."}, operation});
}
} // namespace foxlang::runtime

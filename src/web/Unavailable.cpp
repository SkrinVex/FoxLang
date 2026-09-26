// What the browser playground cannot do: HTTP(S) requests and HTTPS servers need
// libcurl and Mbed TLS, which the WebAssembly build leaves out, and there are no
// processes to start.
#include "foxlang/Platform.h"
#include "../core/TlsServer.h"
#include <cerrno>
#include <spawn.h>
#include <stdexcept>

// A web worker cannot start processes: run_command, os_open and sound players fail
// the way they do when the program is missing.
extern "C" int posix_spawnp(pid_t*, const char*, const posix_spawn_file_actions_t*, const posix_spawnattr_t*,
                            char* const[], char* const[]) {
    return ENOSYS;
}

namespace foxlang::platform {
namespace {
[[noreturn]] void unavailable(const char* what) {
    throw std::runtime_error(std::string("Runtime Error: ") + what +
                             " is not available in the browser playground; run the program with foxlang on a computer");
}
} // namespace

HttpResponse httpRequest(const std::string&, const std::string&, const std::string&, const std::string&) {
    unavailable("HTTP");
}
const char* thirdPartyLicenses() { return ""; }

struct TlsServer::State {};
TlsServer::TlsServer(const std::string&, const std::string&) { unavailable("HTTPS"); }
TlsServer::~TlsServer() = default;
struct TlsSession::State {};
TlsSession::TlsSession(TlsServer&, TlsIo) { unavailable("HTTPS"); }
TlsSession::~TlsSession() = default;
bool TlsSession::handshake() { return false; }
int TlsSession::read(unsigned char*, std::size_t) { return -1; }
int TlsSession::write(const unsigned char*, std::size_t) { return -1; }
} // namespace foxlang::platform

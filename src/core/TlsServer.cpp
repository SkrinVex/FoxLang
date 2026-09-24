#include "TlsServer.h"
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>
#include <stdexcept>

namespace foxlang::platform {
namespace {
void checked(int result, const char* operation) {
    if (result == 0) return;
    char message[256]{};
    mbedtls_strerror(result, message, sizeof(message));
    throw std::runtime_error(std::string("HTTPS Server Error: ") + operation + ": " + message);
}
bool retry(int result) { return result == MBEDTLS_ERR_SSL_WANT_READ || result == MBEDTLS_ERR_SSL_WANT_WRITE; }
}

struct TlsServer::State {
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context random;
    mbedtls_x509_crt certificate;
    mbedtls_pk_context key;
    mbedtls_ssl_config config;
    State() {
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&random);
        mbedtls_x509_crt_init(&certificate);
        mbedtls_pk_init(&key);
        mbedtls_ssl_config_init(&config);
    }
    ~State() {
        mbedtls_ssl_config_free(&config);
        mbedtls_pk_free(&key);
        mbedtls_x509_crt_free(&certificate);
        mbedtls_ctr_drbg_free(&random);
        mbedtls_entropy_free(&entropy);
    }
};

TlsServer::TlsServer(const std::string& certificate, const std::string& privateKey)
    : state(std::make_unique<State>()) {
    if (certificate.empty() || privateKey.empty() || certificate.find('\0') != std::string::npos ||
        privateKey.find('\0') != std::string::npos) throw std::runtime_error("HTTPS Server Error: certificate and private key paths are required");
    // PSA initialization is idempotent; shared TLS clients may also use it.
    if (psa_crypto_init() != PSA_SUCCESS) throw std::runtime_error("HTTPS Server Error: crypto initialization failed");
    const unsigned char personalization[] = "FoxLang HTTPS server";
    checked(mbedtls_ctr_drbg_seed(&state->random, mbedtls_entropy_func, &state->entropy,
                                personalization, sizeof(personalization) - 1), "initialize random generator");
    checked(mbedtls_x509_crt_parse_file(&state->certificate, certificate.c_str()), "load certificate");
    checked(mbedtls_pk_parse_keyfile(&state->key, privateKey.c_str(), nullptr,
                                    mbedtls_ctr_drbg_random, &state->random), "load private key");
    checked(mbedtls_pk_check_pair(&state->certificate.pk, &state->key,
                                 mbedtls_ctr_drbg_random, &state->random), "certificate/private key mismatch");
    checked(mbedtls_ssl_config_defaults(&state->config, MBEDTLS_SSL_IS_SERVER,
                                        MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT), "configure TLS");
    mbedtls_ssl_conf_min_tls_version(&state->config, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_rng(&state->config, mbedtls_ctr_drbg_random, &state->random);
    checked(mbedtls_ssl_conf_own_cert(&state->config, &state->certificate, &state->key), "configure certificate");
}
TlsServer::~TlsServer() = default;

struct TlsSession::State {
    mbedtls_ssl_context ssl;
    TlsIo io;
    bool connected = false;
    explicit State(TlsIo callbacks) : io(callbacks) { mbedtls_ssl_init(&ssl); }
    ~State() {
        if (connected) mbedtls_ssl_close_notify(&ssl);
        mbedtls_ssl_free(&ssl);
    }
    static int receive(void* opaque, unsigned char* bytes, std::size_t size) {
        auto& io = static_cast<State*>(opaque)->io;
        int result = io.read(io.context, bytes, size);
        return result < 0 ? MBEDTLS_ERR_NET_RECV_FAILED : result;
    }
    static int send(void* opaque, const unsigned char* bytes, std::size_t size) {
        auto& io = static_cast<State*>(opaque)->io;
        int result = io.write(io.context, bytes, size);
        return result < 0 ? MBEDTLS_ERR_NET_SEND_FAILED : result;
    }
};

TlsSession::TlsSession(TlsServer& server, TlsIo io) : state(std::make_unique<State>(io)) {
    checked(mbedtls_ssl_setup(&state->ssl, &server.state->config), "initialize connection");
    mbedtls_ssl_set_bio(&state->ssl, state.get(), &State::send, &State::receive, nullptr);
}
TlsSession::~TlsSession() = default;
bool TlsSession::handshake() {
    int result;
    do { result = mbedtls_ssl_handshake(&state->ssl); } while (retry(result));
    state->connected = result == 0;
    return state->connected;
}
int TlsSession::read(unsigned char* bytes, std::size_t size) {
    int result;
    do { result = mbedtls_ssl_read(&state->ssl, bytes, size); } while (retry(result));
    return result;
}
int TlsSession::write(const unsigned char* bytes, std::size_t size) {
    int result;
    do { result = mbedtls_ssl_write(&state->ssl, bytes, size); } while (retry(result));
    return result;
}
}

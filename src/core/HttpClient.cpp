#include "foxlang/Platform.h"
#include <curl/curl.h>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include "NetworkLicenses.h"
#include "EmbeddedCertificates.h"

namespace foxlang::platform {
namespace {
struct CurlRuntime {
    CurlRuntime() {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) throw std::runtime_error("HTTP Error: initialization failed");
    }
    ~CurlRuntime() { curl_global_cleanup(); }
};

std::size_t receive(char* bytes, std::size_t size, std::size_t count, void* opaque) noexcept {
    auto& body = *static_cast<std::string*>(opaque);
    constexpr std::size_t limit = 16 * 1024 * 1024;
    if (size && count > (limit - body.size()) / size) return 0;
    try { body.append(bytes, size * count); } catch (...) { return 0; }
    return size * count;
}
}

std::string httpRequest(const std::string& method, const std::string& url,
                        const std::string& body, const std::string& contentType,
                        bool failOnHttpError) {
    static CurlRuntime runtime;
    if (url.find('\0') != std::string::npos || contentType.find_first_of("\r\n") != std::string::npos ||
        contentType.find('\0') != std::string::npos) throw std::runtime_error("HTTP Error: invalid URL or Content-Type");
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> request(curl_easy_init(), curl_easy_cleanup);
    if (!request) throw std::runtime_error("HTTP Error: cannot create request");
    auto option = [&](CURLoption key, auto value) {
        if (curl_easy_setopt(request.get(), key, value) != CURLE_OK) throw std::runtime_error("HTTP Error: cannot configure request");
    };
    std::string response;
    option(CURLOPT_URL, url.c_str());
    option(CURLOPT_PROTOCOLS_STR, "http,https");
    option(CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
    option(CURLOPT_CONNECTTIMEOUT, 10L);
    option(CURLOPT_TIMEOUT, 35L);
    option(CURLOPT_NOSIGNAL, 1L);
    option(CURLOPT_WRITEFUNCTION, &receive);
    option(CURLOPT_WRITEDATA, &response);
    option(CURLOPT_SSL_VERIFYPEER, 1L);
    option(CURLOPT_SSL_VERIFYHOST, 2L);
#ifdef _WIN32
    // Private/offline CAs may publish no revocation endpoint. Keep chain/hostname
    // verification and reject known revocations, without requiring an online CRL.
    option(CURLOPT_SSL_OPTIONS, static_cast<long>(CURLSSLOPT_REVOKE_BEST_EFFORT));
#endif
    option(CURLOPT_FAILONERROR, failOnHttpError ? 1L : 0L);
    option(CURLOPT_CUSTOMREQUEST, method.c_str());
    auto ca = getEnvVar("FOXLANG_CA_BUNDLE");
#ifndef _WIN32
    if (ca.empty()) ca = getEnvVar("SSL_CERT_FILE");
#endif
    if (ca.empty() || ca == "embedded") {
        curl_blob trust{const_cast<unsigned char*>(bundledCaCertificates), sizeof(bundledCaCertificates) - 1, CURL_BLOB_NOCOPY};
        option(CURLOPT_CAINFO_BLOB, &trust);
    } else if (ca == "system") {
#ifndef _WIN32
        ca.clear();
        for (const auto* candidate : {"/etc/ssl/certs/ca-certificates.crt", "/etc/pki/tls/certs/ca-bundle.crt", "/etc/ssl/cert.pem"}) {
            std::error_code ec;
            if (std::filesystem::is_regular_file(candidate, ec)) { ca = candidate; break; }
        }
        if (ca.empty()) throw std::runtime_error("HTTP Error: system CA store not found");
        option(CURLOPT_CAINFO, ca.c_str());
#endif
    } else {
        option(CURLOPT_CAINFO, ca.c_str());
    }
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers(nullptr, curl_slist_free_all);
    if (method == "POST" || method == "PUT") {
        auto header = "Content-Type: " + contentType;
        headers.reset(curl_slist_append(nullptr, header.c_str()));
        if (!headers) throw std::runtime_error("HTTP Error: cannot create headers");
        option(CURLOPT_HTTPHEADER, headers.get());
        option(CURLOPT_POSTFIELDS, body.data());
        option(CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
    }
    auto result = curl_easy_perform(request.get());
    if (result != CURLE_OK) {
        // Do not log the URL or body: webhook URLs often contain secret tokens.
        throw std::runtime_error("HTTP Error: " + method + ": " + curl_easy_strerror(result));
    }
    return response;
}

const char* thirdPartyLicenses() { return networkLicenses(); }
} // namespace foxlang::platform

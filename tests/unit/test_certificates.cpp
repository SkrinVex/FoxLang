#include "EmbeddedCertificates.h"
#include <mbedtls/x509_crt.h>
#include <iostream>

int main() {
    mbedtls_x509_crt roots;
    mbedtls_x509_crt_init(&roots);
    int result = mbedtls_x509_crt_parse(&roots, reinterpret_cast<const unsigned char*>(bundledCaCertificates), sizeof(bundledCaCertificates));
    if (result != 0) { std::cerr << "Invalid embedded trust store: " << result << '\n'; return 1; }
    int count = 0;
    for (auto* cert = &roots; cert; cert = cert->next) {
        if (cert->raw.len == 0 || mbedtls_x509_crt_check_key_usage(cert, MBEDTLS_X509_KU_KEY_CERT_SIGN) != 0) {
            mbedtls_x509_crt_free(&roots); return 1;
        }
        ++count;
    }
    mbedtls_x509_crt_free(&roots);
    // All roots in the pinned Mozilla snapshot must survive byte-for-byte embedding.
    if (count != 121) { std::cerr << "Unexpected CA count: " << count << '\n'; return 1; }
    std::cout << "EMBEDDED_MOZILLA_CA_OK\n";
}

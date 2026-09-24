# Embedded public trust store

`mozilla.pem` is the unmodified Mozilla CA snapshot distributed by curl:

- Source: https://curl.se/ca/cacert-2026-08-13.pem
- Published SHA-256: https://curl.se/ca/cacert-2026-08-13.pem.sha256
- SHA-256: `f66dff1bdf8f96060b8177976f8b7d9254bc89bc4db933d769f7384d28480bc9`
- Snapshot: 2026-08-13, 121 public roots, MPL-2.0 (see LICENSE).

CMake verifies the hash and embeds these public certificates as bytes. They are
not private keys and are unrelated to a developer's local trust store or `.env`.
The conversion retains certificate constraints encoded in X.509, but does not
include all additional browser-specific Mozilla trust policy; see
https://curl.se/docs/caextract.html.

To update, download a dated official snapshot and its published checksum, compare
the roots added/removed, and update the pinned hash in `cmake/Networking.cmake`,
the expected count in `tests/unit/test_certificates.cpp`, this provenance and the
snapshot date in the embedded license notice. Run the full network tests. Never
copy arbitrary certificates from the build machine into this directory.

Rebuild FoxLang and repackage applications to distribute a reviewed trust-store
update. At runtime `FOXLANG_CA_BUNDLE=/path/to/ca.pem` replaces the default trust
store; `system` explicitly uses OS trust, and `embedded` explicitly uses this
snapshot. Linux also honors SSL_CERT_FILE when FOXLANG_CA_BUNDLE is unset.
No online trust-store updater or silent verification bypass is provided.

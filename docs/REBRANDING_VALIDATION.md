# Skrumi migration verification

## Locally verified

The native Linux Release build and **all 116 CTest tests passed**. This includes
VM/container/reference/copy/type/error regressions, language programs, benchmark
smoke tests, stdlib and example checks, documentation examples, real LSP sessions,
network/TLS, debugger, standalone bundles, graphics/input/window modes, CLI aliases,
mixed extensions and isolated installation checks. No test was skipped in the
final run. The final window-mode test runs both legacy and new raycaster sources.

Commands (from the repository root):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSKRUMI_LTO=OFF
cmake --build build -j4
ctest --test-dir build --output-on-failure -j4
python3 tests/test_rebranding.py build/skrumi
python3 tests/test_installation.py build/skrumi
python3 tests/test_lsp_project.py build/skrumi-lsp mixed
python3 tests/test_android_packaging.py
python3 tests/test_android_trace.py
python3 tests/test_versions.py .
python3 packaging/sync_editor_builtins.py --check
python3 packaging/site_facts.py --build build
```

This workspace did not permit system package installation. CMake/Ninja were
installed in the user tool directory; XCB headers and X11/Openbox packages were
extracted to `/tmp/skrumi-sysroot`. Native configure used
`PKG_CONFIG_PATH=/tmp/skrumi-sysroot/usr/lib/x86_64-linux-gnu/pkgconfig:/tmp/skrumi-sysroot/usr/share/pkgconfig`.
The final test run used `DISPLAY=:98` (Xvfb), the sysroot's `usr/bin` on PATH and
its library directory in `LD_LIBRARY_PATH`; Openbox was started with its sysroot
share/config directories. On an ordinary Linux machine, install the development
packages documented in README plus Xvfb/Openbox and run under `xvfb-run -a`.
LTO was disabled locally; CI retains the normal Release/LTO configuration.

A second CMake configure with Ninja and `BUILD_TESTING=OFF` checked generator
compatibility. It caught and fixed an output/phony-target collision; modern CMake
names are aliases of the original targets. Existing `--target foxlang` builds
remain valid. A full native build was performed with Unix Makefiles.

## Browser and WASM

Emscripten **6.0.10**, matching CI, built both normal and Asyncify graphics runtimes.
Both source suffixes execute through the real WASM engine. The following passed:

```sh
emcmake cmake -S . -B web -DCMAKE_BUILD_TYPE=Release
cmake --build web -j4
cp web/skrumi*.js web/skrumi*.wasm web/foxlang*.js web/foxlang*.wasm site/playground/
node tests/web/test_playground.js site/playground
node tests/web/test_builder_model.js
node tests/web/test_builder_runtime.js site/playground
npm ci --prefix tests/web
# Install Chromium once: cd tests/web && npx playwright install chromium
npm test --prefix tests/web
node tests/web/test_builder.js --runtime
```

Chromium verified desktop/mobile editing, input and builder controls, plus the
builder with real graphics WASM. Additional checks cover old saved `.fox` projects
with `.skm` imports and preservation of old builder source. Old WASM asset names
are copies of the new build for cached pages and external callers.

## Platform boundaries

Windows native execution/installer tests, Android AAR/APK compilation and emulator
execution were **not performed locally**: the corresponding toolchains/emulators
are not installed here. The PR retains and updates Windows CI, PowerShell 5.1/7
installer checks, four-ABI Android packaging/16 KB checks, and Android API 28/35
instrumentation. The Android tests now exercise both Java APIs and mixed sources.
The Windows installer records file hashes and preserves shared editor extensions.
Review those CI results before merging/releasing; do not treat local Python APK
staging tests as a real Android build.

Zed/Rust extension compilation, marketplace publication, Docker/musl build and a
real signed Android update were not run locally. Existing CI covers Docker/musl.
Published extension IDs, Android application IDs and JNI contracts were preserved.
No release, tag, domain, remote repository name or marketplace identity was changed.

## Final audit and release checklist

`git diff --check`, Python syntax compilation, JavaScript syntax checks, workflow
YAML parsing, metadata/VSIX checks and repeated legacy-name scans were performed.
See [REBRANDING_AUDIT.md](REBRANDING_AUDIT.md) for why remaining identifiers stay.
The changelog has an Unreleased entry; `VERSION` remains unchanged.

After reviewing platform CI, choose the next version using the existing release
policy, update VERSION, and let the release workflow produce `Skrumi-*` archives,
new executable names, SDK assets and compatibility download aliases. Verify
install/update/uninstall on clean Windows/Linux profiles, all four CLI/file
combinations from installed packages, Pages and fixed download links. Publish the
editor update under its existing marketplace identity. Keep signing credentials,
application IDs, Git history, tags and old releases unchanged. A later GitHub
repository rename requires a separate owner decision and a Pages/URL plan.

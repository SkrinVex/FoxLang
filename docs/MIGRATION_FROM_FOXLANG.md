# Migrating from FoxLang to Skrumi

Skrumi is the continuation of FoxLang by **SkrinVex**. The name changes to give
all parts of the project one identity; this is not a new language or a rewrite.
The Git history, license, previous releases and language semantics are retained.
The version remains governed by `VERSION`; rebranding does not reset its numbering.

## Commands and source files

The recommended commands are `skrumi main.skm` and `skrumi-lsp`. These all use the
same engine:

```sh
skrumi main.skm
skrumi main.fox
foxlang main.fox
foxlang main.skm
```

Existing source files do **not** need to be renamed or edited. `.skm` and `.fox`
share the parser, VM, formatter, test runner, project index and module system.
A project may contain both. `include("helper.fox")` and `include("helper.skm")`
refer to precisely the named file: no silent extension substitution.

For `using helper;`, the existing standard-library precedence is retained. Within
standard-module lookup and then ordinary module lookup, the complete legacy
`.fox` search precedes `.skm` fallback. Thus adding `helper.skm` cannot silently
replace an existing `helper.fox`. Prefer explicit `include(...)` when both exist.
Embedded standard modules keep their historical `.fox` identities. A module's
canonical identity still prevents repeated loading.

To migrate voluntarily: update your entry command, rename files, and update every
explicit include/path that refers to a renamed file. Migrate incrementally; test
with `skrumi check`, your existing tests, and `skrumi test`. New examples use `.skm`;
old examples remain available to preserve links.

## Installation and settings

New packages use `Skrumi-...` names. CMake installs `skrumi`, `skrumi-lsp` and
compatible `foxlang`, `foxlang-lsp` executables, built once from the same sources.
The Linux installer uses `share/skrumi`, Windows uses `%LOCALAPPDATA%\Skrumi`.
Installers refuse unmanaged destination directories and refuse upgrades/uninstalls when installed files were modified or additional files were added. Preserve such custom data and choose a clean destination. Old installation directories
are preserved. Ensure the new bin directory precedes an old installation in PATH;
use `command -v skrumi foxlang` (Linux) or `Get-Command skrumi,foxlang` (PowerShell).
Do not run an old uninstaller against launchers shared with a newer installation.

Runtime `SKRUMI_HOME`, `SKRUMI_LOG_LEVEL`, `SKRUMI_CA_BUNDLE`,
`SKRUMI_ABSOLUTE_PATHS` and `SKRUMI_X11_SHM` take precedence over the corresponding
`FOXLANG_*` variables, including when explicitly empty. Ordinary program environment
lookups remain literal. `--skrumi-*` runtime options alias all existing
`--foxlang-*` options; `--` still protects literal program arguments. Neither CLI
alias emits migration notices that might corrupt script output or LSP framing.
CMake accepts `SKRUMI_STATIC_LINUX`, `SKRUMI_LTO`, `SKRUMI_BUILD_ANDROID_JNI` as
preferred aliases for the corresponding old options.

VS Code retains its published `SkrinVex.foxlang-language` identity, `fox` language
ID, `foxlang.*` settings/commands, task/debug types and syntax scopes. Both suffixes
activate the same tools. Existing configured executable paths continue to work.
Zed retains its `FoxLang` language settings key, extension and adapter identifiers; Kate retains configuration keys.
These technical identifiers are intentional compatibility interfaces.

## Embedding and Android

C++ may use `#include <skrumi/Skrumi.h>` and `skrumi::Interpreter`; existing
`foxlang` headers, namespace and CMake targets remain available. They name the same
implementation. Both `libskrumi_core` and the legacy `libfoxlang_core` archive are installed (with platform-specific suffixes). `skrumi_core` is a CMake alias of `foxlang_core`.

Android's recommended facade is `org.skrumi.android.Skrumi`, with `SkrumiView` and
`SkrumiControls`. The original `org.foxlang.android` API, JNI symbols,
`libfoxlang_android.so`, Gradle `:foxlang` module and entry manifest remain supported.
The new facade shares the original worker and native engine. Both extensions are
staged into APKs. Existing application IDs are unchanged, including generated app
and demo defaults. Keep your signing key and application ID for updates.
Gradle accepts `skrumi*` properties with `foxlang*` fallbacks and `SKRUMI_NDK_DIR`
with `FOXLANG_NDK_DIR` fallback.

## Website, saved projects and releases

The existing GitHub repository, Pages address and external URLs remain in use.
No new domain is assumed. Playground accepts both suffixes and old shared projects;
builder JSON, local storage keys and message protocols remain compatible. New
projects use `.skm`. The WASM C exports retain their names for compatibility;
new generated bundles are named `skrumi.js` and `skrumi-graphics.js`.

Before release, run native CTest, Windows installer CI, Android emulator/AAR/APK
checks, and the browser/WASM jobs. See [migration verification](REBRANDING_VALIDATION.md)
for actual local results and limitations. Publish compatibility copies of fixed
old download names alongside new artifacts so existing `releases/latest` links
continue working. Keep old releases and tags intact.

The owner may later rename the GitHub repository separately. Before doing so,
review Pages routing (repository redirects do not guarantee Pages redirects),
badges, workflow references, GHCR coordinates and download links. No repository
rename, marketplace publication, tag creation or release publication is part of
this source migration.

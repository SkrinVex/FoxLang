# Legacy identifiers retained intentionally

The migration scans both tracked file contents and paths, including dotfiles,
for `FoxLang`, `foxlang`, `FOXLANG`, `.fox` and related identifiers. Build trees,
downloaded dependencies, generated WASM and `.git` objects are excluded.

The remaining names are compatibility or historical contracts, not an alternate
implementation. Their reasons are:

| Area | Retained names and reason |
| --- | --- |
| GitHub, Pages, badges, download URLs | After the owner renamed the repository to `SkrinVex/Skrumi`, active links use that repository and `skrinvex.github.io/Skrumi`. Historical links and `.fox` examples retain the old repository URL, which GitHub redirects. Old fixed VSIX/APK/AAR download names remain compatible. The README version badge is local and synchronized from VERSION. No new domain is assumed. |
| History and legal notices | Previous changelog entries, LICENSE, bundled license heading, historical benchmark data and old release versions remain factual. |
| C++ API / ABI | `include/foxlang`, `foxlang::`, `FoxLang.h`, existing fields such as `foxHome`, build target names and implementation macros. New `include/skrumi` headers and namespace aliases use the same classes. Both library filenames are installed. |
| Native binary and bundle format | `foxlang`, `foxlang-lsp` aliases; bundle descriptor symbols/magic, internal synthetic `.fox` identities and Windows `FoxLangGraphicsWindow` class remain stable for existing tools. |
| Runtime configuration | `FOXLANG_*`, `--foxlang-*`, legacy install paths and `FOXLANG_PREFIX` fallback remain accepted. Test-only environment switches retain their names. New settings and runtime help recommend Skrumi. |
| Sources and module lookup | `.fox` standard library, its embedded identities and legacy source copies remain supported. Extensionless `using` retains legacy precedence. `.skm` examples are the recommended current copies. |
| Regression suite / benchmarks | Original `.fox` fixtures and their paths, old API coverage, migration tests, benchmark folder/JSON schema keys and baseline metadata remain stable. Literal strings in programs are test data. Assertions on actual new CLI branding were updated. |
| Android | `org.foxlang.android`, `org.foxlang.runner`, application IDs, Gradle module `:foxlang`, `foxlang*` property fallbacks, JNI exports, `libfoxlang_android.so`, `foxlang-entry.txt` and Intent extra `foxlang.arguments`. Changing these would break SDK consumers or app updates. Skrumi facades delegate to this implementation. |
| Android profiling | `FoxLang.present`, `FoxLang.frame.copy`, `FoxView.upload/draw` trace identifiers and historical captures remain readable by the profiler. |
| VS Code | Published `SkrinVex.foxlang-language` identity, `fox` language ID, `source.fox` scopes, `foxlang.*` settings/commands, tasks/debug adapter/problem matcher IDs. Old configured paths and old diagnostic prefixes remain recognized. Both suffixes use the same registration. |
| Zed | Extension/crate ID `foxlang`, persisted language key `FoxLang`, `foxlang-lsp` server registration and `FoxLang` debug adapter/schema identity. Display brand is Skrumi; executable lookup prefers Skrumi and falls back to legacy commands. |
| Kate | Existing config keys, syntax/tool filenames and action names remain stable for shortcuts and existing settings. Display labels and executables are updated; source filter includes both suffixes. |
| Browser | Storage keys, `FoxBuilder` global, builder message protocol, shared-project source names, `createFoxLang` factory and C exports remain compatible. Generated bundles have new names plus old URL copies. CSS classes and benchmark JSON keys are internal identifiers. |
| Markdown language labels | `foxlang` code-fence identifiers are retained for existing highlighters. They are not a second parser. |
| Temporary runtime files | Legacy temp-file prefixes are private implementation identifiers; changing them is unnecessary for the user-facing brand and could disrupt cleanup/diagnostic tooling. |

No old releases, tags, licenses or Git history were removed. The final verification
commands and untested platform boundaries are recorded in
[REBRANDING_VALIDATION.md](REBRANDING_VALIDATION.md).

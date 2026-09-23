# Standalone executables

`foxlang build main.fox -o app` packages the native FoxLang executable, the entry
source and its source dependencies into one file. Linux produces ELF64 x86_64;
Windows produces PE32+ x86_64 (`app.exe`). Neither packaging nor running the result
requires a compiler, CMake, a FoxLang installation or a separate FoxLang runtime.
This is runtime bundling, not native AOT compilation.

## Architecture

```text
foxlang_core (Lexer, Parser, Interpreter, Runtime, Platform)
    + SourceProvider: filesystem + embedded standard library
    + SourceProvider: immutable in-memory bundle
               |
      native foxlang executable
        ├── empty descriptor: CLI, including build
        └── populated descriptor: execute bundled entry with Interpreter
```

The CLI itself is the prebuilt runtime stub. This avoids a second executable that
installers must locate and version-match. The resulting application contains the
same core and the packaging code, but always runs its embedded entry, including
when passed `--help`, `--version`, or `build`. There is no second interpreter and
no extraction to disk. Application arguments are not exposed by the current
FoxLang language API.

The running image is located using `/proc/self/exe` on Linux and
`GetModuleFileNameW` on Windows, independently of `argv[0]`, PATH, cwd and symlinks.
The builder validates the executable section tables before modifying the reserved
descriptor. See the [ELF ABI specification](https://refspecs.linuxfoundation.org/elf/gabi41.pdf)
and [Microsoft PE specification](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)
for the host formats. This implementation deliberately accepts only little-endian
ELF64/AMD64 and PE32+/AMD64 executables with a `.foxbndl` section.

The build reads/parses dependencies without evaluating user code, serializes and
checks the payload, writes into a uniquely created temporary directory beside the
destination, re-reads and validates the file, then publishes it atomically without
overwriting an existing destination. Temporary files are cleaned up with RAII.
Linux publication uses a hard link within that filesystem; Windows uses
`MoveFileW`. A filesystem without hard-link support cannot publish on Linux.
An existing output must be removed or renamed explicitly before rebuilding.

## Format version 1

All integers are unsigned little-endian and are read byte by byte, without casting
untrusted data to C++ structs. Names and sources are length-delimited byte strings
(normally UTF-8). No marker search locates the payload.

The `.foxbndl` section begins with this 40-byte descriptor:

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 8 | `FOXSTUB` followed by NUL |
| 8 | 4 | descriptor version: 1 |
| 12 | 4 | mode: 0 for CLI, 1 for bundled application |
| 16 | 8 | absolute file offset of payload |
| 24 | 8 | payload byte length |
| 32 | 4 | IEEE CRC32 of entire payload |
| 36 | 4 | reserved, must be zero |

For a CLI stub, offset, length and checksum must all be zero. For an application,
the payload must start after all executable tables and file-backed sections, and
end exactly at EOF. This descriptor remains present if a transfer truncates the
payload, so the application fails with `Bundle Error` instead of becoming a CLI.

Payload layout:

```text
8 bytes: "FOXBNDL\0"
u32: version (1)
u32: file count
u32: import edge count
string: entry source ID
repeat file count:
    string: source ID
    string: source bytes
repeat import edge count:
    string: importing source ID
    u32: kind (0 = include, 1 = using)
    string: original requested module name
    string: resolved target source ID
```

Every string is `u32 byte_length` followed by exactly that many bytes. IDs such as
`main.fox` and `module/1.fox` are opaque map keys, never paths for extraction.
Canonical developer paths are not stored as IDs; paths explicitly written inside
source literals remain source data.

Limits: 64 MiB payload, 256 MiB original executable, 4096 source files, 65536 import
edges, 4096 bytes per name. The reader rejects unsupported versions, invalid
magic, overflow/out-of-range lengths, duplicate files/imports, invalid import
kinds, missing entry/targets, invalid executable section tables, trailing bytes,
and checksum mismatches before interpreting any source.

CRC32 detects accidental corruption; it is not authentication. Embedded sources
are readable, not encrypted. Do not store secrets directly in source code.
PE files with an Authenticode certificate are rejected. Signing, stripping,
compressing or otherwise rewriting an already bundled file is not supported.
If desired, strip the CLI before using it as the stub, preserving `.foxbndl` and
the section table. Signature-aware bundles can be added as a separate format.

## Module resolution

The small official `std/*.fox` library is embedded in `foxlang_core` by CMake.
No user program is turned into generated C++ and no project directory is scanned
for arbitrary files. CMake tracks changes to the standard library sources.

For ordinary CLI execution and packaging, `using name;` resolves `std/name.fox`
then `name.fox`. For each candidate the existing filesystem resolver searches the
importing file's directory, cwd, `FOXLANG_HOME` and `FOXLANG_HOME/std`. Embedded
stdlib is the final fallback. `include("std/json.fox")` also supports that fallback.
A found module's parse/runtime errors propagate instead of being hidden by a
fallback to another file.

The existing parser records all `using` and literal `include` statements,
including statements nested inside functions/branches. Packaging traverses that
graph iteratively, with canonical source identities and a visited map. Cycles
terminate and repeated imports share one source entry. All dependencies must
exist at build time, even imports in unreachable code. Missing imports identify
the request and importing file. Dynamic include expressions are not part of the
current language syntax.

The runtime uses the stored import edges, exclusively. There is no disk fallback,
including if `FOXLANG_HOME` or the recipient's cwd contains similarly named files.
`include`/`using` preserve the existing declaration-only import behavior:
functions, variable initializers and nested imports are loaded; other top-level
statements in a module are skipped. The entry program runs normally. A canonical
entry identity also prevents a cyclic import from re-importing the main program.

## Resources, secrets and operating systems

- `.env`, build-time environment values and arbitrary project files are never
  discovered or embedded automatically. Only parsed source dependencies are read.
- Standalone programs read the process environment at execution time. Automatic
  `.env` loading is disabled; regular CLI source execution keeps its existing
  `.env` behavior. `env`, `secret`, `env_default` and logging use runtime values.
- `read_file`, `write_file` and `append_file` still access the real filesystem,
  with relative paths resolved from the recipient's working directory. Supply
  configuration/data files separately. Source bundling is not resource bundling.
- HTTP clients still invoke an external `curl` through `popen`/`_popen`. `httpget`
  and `httppost` need curl supporting `--fail-with-body`; HTTPS also needs trust
  certificates. These are runtime dependencies when using HTTP, not part of the
  bundle. No Internet requests are made by the tests.
- HTTP server and TCP/DNS use POSIX sockets. Windows networking support remains
  limited to the curl HTTP client: the server reports unsupported, TCP/DNS retain
  their existing stub results. Packaging does not add Winsock support.
- Terminal APIs still require the relevant console/TTY and ANSI support.
- MSVC uses `/MT` (static CRT); MinGW statically links compiler support libraries.
  Normal OS DLLs are still required. MinGW builds using UCRT target Windows with
  that system component (normally Windows 10 or later).
- Linux/GCC statically links libstdc++/libgcc; libc, libm and the ELF loader remain
  system dependencies. Use a compatible target libc/version and architecture.
  Build FoxLang on the oldest intended target OS; glibc/musl are not interchangeable.
  Other toolchains can retain additional system libraries: inspect their binaries.
- `/proc` must be mounted on Linux. Native packaging only; no cross-target flag.

## Verification

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
ctest --test-dir build -C Release -L standalone --output-on-failure
```

Python 3 is required for tests only. CI runs the same suite in `desktop-linux`
and `desktop-windows`; the POSIX server test runs only on Linux. The integration
tests copy only the CLI into a temporary developer directory (without stdlib),
package source fixtures, copy only the resulting executable into a separate
recipient directory, delete both sources and copied CLI, and execute there.
PATH is empty except in the explicit curl test. Tests cover exact stdout, CLI
failures, all stdlib imports, transitive/local/cyclic imports, JSON/Unicode,
environment timing, secret/resource exclusion, runtime errors, corrupted images,
and local HTTP client/server with a Telegram-style Unicode webhook.

The C++ format test also checks malformed payloads and native/PE stub structures,
including every truncation position in a sample payload. The hello integration
test prints the byte size of the generated executable (`ctest -V -R standalone_hello`).

## Future native compiler

A genuine AOT pipeline would need semantic analysis and type checking suitable
for compilation, a defined IR, native code generation, runtime ABI/linking,
platform backends, debug information and conformance tests against the interpreter.
Bundle serialization is deliberately independent of such a future backend.

cmake_minimum_required(VERSION 3.18)
cmake_policy(SET CMP0091 NEW)
set(FOXLANG_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
include(cmake/Version.cmake)
project(FoxLang VERSION ${FOXLANG_VERSION} LANGUAGES C CXX)
option(FOXLANG_STATIC_LINUX "Link Linux executables statically (use a musl toolchain for portability)" OFF)
if(FOXLANG_STATIC_LINUX)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "FOXLANG_STATIC_LINUX requires a Linux toolchain")
    endif()
    add_link_options(-static)
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")

# Standalone programs must not need a separately installed MSVC runtime.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
include(cmake/EmbedStdlib.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable compiler warnings
if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/W4> $<$<COMPILE_LANGUAGE:CXX>:/EHsc> $<$<COMPILE_LANGUAGE:CXX>:/utf-8>)
else()
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wall> $<$<COMPILE_LANGUAGE:CXX>:-Wextra> $<$<COMPILE_LANGUAGE:CXX>:-pedantic>)
endif()

# A Windows thread reserves 1 MB of stack by default where Linux gives 8, so the same
# program could recurse eight times less deeply there. Reserve the same 8 MB.
if(WIN32)
    if(MSVC)
        add_link_options(/STACK:8388608)
    else()
        add_link_options(-Wl,--stack,8388608)
    endif()
endif()

# Reusable FoxLang core library
add_library(foxlang_core STATIC
    src/core/Context.cpp
    src/core/Platform.cpp
    src/core/Network.cpp
    src/core/HttpClient.cpp
    src/core/TlsServer.cpp
    src/core/Runtime.cpp
    src/core/SourceProvider.cpp
    src/core/Lexer.cpp
    src/core/Parser.cpp
    src/core/AST.cpp
    src/core/Operations.cpp
    src/core/Compiler.cpp
    src/core/Vm.cpp
    src/core/Project.cpp
    src/core/HttpServer.cpp
    src/core/Process.cpp
    src/core/Template.cpp
    src/core/Formatter.cpp
    src/core/Resolver.cpp
    src/core/Interpreter.cpp
    src/core/SemanticAnalyzer.cpp
    src/core/builtins/Registry.cpp
    src/core/builtins/Core.cpp
    src/core/builtins/Text.cpp
    src/core/builtins/Collections.cpp
    src/core/builtins/System.cpp
    src/core/builtins/Files.cpp
    src/core/builtins/NetworkBuiltins.cpp
    src/core/builtins/Sound.cpp
    src/graphics/Surface.cpp
    src/graphics/Window.cpp
    src/graphics/Ui.cpp
    src/graphics/Image.cpp
    src/graphics/Builtins.cpp
)

target_include_directories(foxlang_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_include_directories(foxlang_core PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_compile_definitions(foxlang_core PRIVATE FOXLANG_VERSION="${PROJECT_VERSION}")
include(cmake/Networking.cmake)
include(cmake/Graphics.cmake)
if(WIN32)
    # ShellExecuteW opens files with their default application (os_open).
    target_link_libraries(foxlang_core PRIVATE shell32 winmm)
endif()

# FoxLang CLI executable
add_library(foxlang_bundle STATIC
    src/standalone/Bundle.cpp
    src/standalone/Image.cpp
    src/standalone/Build.cpp
)
target_include_directories(foxlang_bundle PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/standalone)
target_link_libraries(foxlang_bundle PUBLIC foxlang_core)
# JSON shared by the language server and the debug adapter
add_library(foxlang_json STATIC src/lsp/Json.cpp)
target_include_directories(foxlang_json PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/lsp)

# Debug Adapter Protocol server inside the foxlang executable (foxlang debug-adapter)
add_library(foxlang_debug STATIC
    src/debug/Channel.cpp
    src/debug/DebugAdapter.cpp
)
target_include_directories(foxlang_debug PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/debug)
target_link_libraries(foxlang_debug PUBLIC foxlang_core foxlang_json)
find_package(Threads REQUIRED)
target_link_libraries(foxlang_debug PUBLIC Threads::Threads)
if(WIN32)
    target_link_libraries(foxlang_debug PUBLIC ws2_32)
endif()

add_executable(foxlang src/cli/main.cpp src/cli/Tools.cpp src/standalone/Stub.cpp)
target_link_libraries(foxlang PRIVATE foxlang_bundle foxlang_debug)
if(WIN32)
    # CommandLineToArgvW: program arguments as UTF-16, converted to UTF-8 for os_args().
    target_link_libraries(foxlang PRIVATE shell32)
endif()
if(MINGW)
    target_link_options(foxlang PRIVATE -static -static-libgcc -static-libstdc++)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(foxlang PRIVATE -static-libgcc -static-libstdc++)
endif()

# FoxLang Language Server executable (LSP)
add_executable(foxlang-lsp
    src/lsp/main.cpp
    src/lsp/Transport.cpp
    src/lsp/DocumentManager.cpp
    src/lsp/LspServer.cpp
)
target_include_directories(foxlang-lsp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/lsp)
target_link_libraries(foxlang-lsp PRIVATE foxlang_core foxlang_json)

# CTest integration
# Release builds optimize across source files when linking: calls between the VM,
# the runtime and the builtins become direct and small helpers are inlined.
option(FOXLANG_LTO "Link-time optimization for release builds" ON)
if(FOXLANG_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT foxlang_ipo OUTPUT foxlang_ipo_error LANGUAGES CXX)
    if(foxlang_ipo)
        set_property(TARGET foxlang_core foxlang_bundle foxlang_json foxlang_debug foxlang foxlang-lsp
                     PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    else()
        message(STATUS "Link-time optimization is not available: ${foxlang_ipo_error}")
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation rules
include(GNUInstallDirs)
install(TARGETS foxlang foxlang-lsp foxlang_core
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/foxlang DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY std DESTINATION ${CMAKE_INSTALL_DATADIR}/foxlang)

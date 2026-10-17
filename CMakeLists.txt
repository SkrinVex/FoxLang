cmake_minimum_required(VERSION 3.18)
cmake_policy(SET CMP0091 NEW)
set(FOXLANG_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
include(cmake/Version.cmake)
# New cache options take precedence; existing -DFOXLANG_* scripts remain valid.
foreach(option STATIC_LINUX LTO BUILD_ANDROID_JNI)
    if(DEFINED SKRUMI_${option})
        set(FOXLANG_${option} "${SKRUMI_${option}}" CACHE BOOL "Skrumi option" FORCE)
    endif()
endforeach()
project(Skrumi VERSION ${FOXLANG_VERSION} LANGUAGES C CXX)
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

if(ANDROID)
    if(ANDROID_PLATFORM_LEVEL LESS 28)
        message(FATAL_ERROR "Skrumi Android requires API 28 or newer (Android 9)")
    endif()
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

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

# Reusable Skrumi core library
set(foxlang_core_sources
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
    src/core/builtins/StrictFiles.cpp
    src/core/builtins/Processes.cpp
    src/core/builtins/Directories.cpp
    src/core/builtins/UserEnvironment.cpp
    src/core/builtins/NetworkBuiltins.cpp
    src/core/builtins/Sound.cpp
    src/graphics/Font.cpp
    src/graphics/Surface.cpp
    src/graphics/Window.cpp
    src/graphics/Ui.cpp
    src/graphics/Image.cpp
    src/graphics/Builtins.cpp
)
if(EMSCRIPTEN)
    # The browser playground: no libcurl or Mbed TLS, so no HTTP client or HTTPS server.
    list(REMOVE_ITEM foxlang_core_sources src/core/HttpClient.cpp src/core/TlsServer.cpp)
    list(APPEND foxlang_core_sources src/web/Unavailable.cpp)
endif()
if(ANDROID)
    list(APPEND foxlang_core_sources src/android/Host.cpp)
endif()
add_library(foxlang_core STATIC ${foxlang_core_sources})
add_library(skrumi_core ALIAS foxlang_core)
set_target_properties(foxlang_core PROPERTIES OUTPUT_NAME skrumi_core)
# Keep the static library filename used by existing native consumers.
add_custom_command(TARGET foxlang_core POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:foxlang_core>
            $<TARGET_FILE_DIR:foxlang_core>/${CMAKE_STATIC_LIBRARY_PREFIX}foxlang_core${CMAKE_STATIC_LIBRARY_SUFFIX})

target_include_directories(foxlang_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_include_directories(foxlang_core PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_compile_definitions(foxlang_core PRIVATE FOXLANG_VERSION="${PROJECT_VERSION}")
include(cmake/Fonts.cmake)
if(EMSCRIPTEN)
    # WebAssembly for the playground page (site/playground): the page loads
    # foxlang.js/foxlang.wasm into a web worker. Nothing else is built.
    target_compile_options(foxlang_core PUBLIC -fwasm-exceptions)
    set(foxlang_web_options -fwasm-exceptions
        -sMODULARIZE=1 -sEXPORT_NAME=createFoxLang -sENVIRONMENT=worker,node
        -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=512MB -sSTACK_SIZE=4MB
        -sEXPORTED_RUNTIME_METHODS=ccall,FS -sEXIT_RUNTIME=0 -sINVOKE_RUN=0)
    add_executable(foxlang_web src/web/Playground.cpp)
    target_link_libraries(foxlang_web PRIVATE foxlang_core)
    set_target_properties(foxlang_web PROPERTIES OUTPUT_NAME skrumi SUFFIX .js)
    target_link_options(foxlang_web PRIVATE ${foxlang_web_options})
    # Programs with a window: every window_poll() and wait() hands the time back to
    # the browser, which delivers the keys and the mouse. Asyncify makes that
    # possible and costs speed and size, so only such programs load this build.
    # Asyncify cannot unwind through WebAssembly exceptions, so this build's core
    # uses Emscripten's JavaScript ones.
    add_library(foxlang_core_graphics STATIC ${foxlang_core_sources})
    target_link_libraries(foxlang_core_graphics PRIVATE freetype)
    target_include_directories(foxlang_core_graphics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
                               PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
    target_compile_definitions(foxlang_core_graphics PRIVATE FOXLANG_VERSION="${PROJECT_VERSION}")
    target_compile_options(foxlang_core_graphics PUBLIC -fexceptions)
    add_executable(foxlang_web_graphics src/web/Playground.cpp)
    target_link_libraries(foxlang_web_graphics PRIVATE foxlang_core_graphics)
    set_target_properties(foxlang_web_graphics PROPERTIES OUTPUT_NAME skrumi-graphics SUFFIX .js)
    target_link_options(foxlang_web_graphics PRIVATE -fexceptions
        -sMODULARIZE=1 -sEXPORT_NAME=createFoxLang -sENVIRONMENT=worker,node
        -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=512MB -sSTACK_SIZE=4MB
        -sEXPORTED_RUNTIME_METHODS=ccall,FS -sEXIT_RUNTIME=0 -sINVOKE_RUN=0
        -sASYNCIFY=1 "-sASYNCIFY_IMPORTS=['fox_canvas_poll','fox_pause']" -sASYNCIFY_STACK_SIZE=1048576)
    # Cached old pages/workers can still fetch the previous bundle URLs.
    foreach(flavor "" "-graphics")
        if(flavor STREQUAL "")
            set(web_target foxlang_web)
        else()
            set(web_target foxlang_web_graphics)
        endif()
        add_custom_command(TARGET ${web_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${web_target}>
                    ${CMAKE_CURRENT_BINARY_DIR}/foxlang${flavor}.js
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/skrumi${flavor}.wasm
                    ${CMAKE_CURRENT_BINARY_DIR}/foxlang${flavor}.wasm)
    endforeach()
    return()
endif()
include(cmake/Networking.cmake)
include(cmake/Graphics.cmake)
include(GNUInstallDirs)
install(FILES $<TARGET_FILE_DIR:foxlang_core>/${CMAKE_STATIC_LIBRARY_PREFIX}foxlang_core${CMAKE_STATIC_LIBRARY_SUFFIX}
        DESTINATION ${CMAKE_INSTALL_LIBDIR})
if(ANDROID)
    option(FOXLANG_BUILD_ANDROID_JNI "Build the Java/Kotlin Android bridge" ON)
    if(FOXLANG_BUILD_ANDROID_JNI)
        add_library(foxlang_android SHARED src/android/Jni.cpp)
        target_include_directories(foxlang_android PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
        target_link_libraries(foxlang_android PRIVATE foxlang_core log android)
        target_link_options(foxlang_android PRIVATE "-Wl,-z,max-page-size=16384")
        install(TARGETS foxlang_android LIBRARY DESTINATION lib)
    endif()
    install(TARGETS foxlang_core ARCHIVE DESTINATION lib)
    install(DIRECTORY include/skrumi include/foxlang DESTINATION include)
    return()
endif()

if(WIN32)
    # ShellExecuteW opens files with their default application (os_open).
    target_link_libraries(foxlang_core PRIVATE shell32 winmm ole32 advapi32 uuid)
endif()

# Skrumi CLI executable
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

add_executable(foxlang src/cli/main.cpp src/cli/Launch.cpp src/cli/Help.cpp src/cli/Tools.cpp src/standalone/Stub.cpp)
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

# Skrumi Language Server executable (LSP)
add_executable(foxlang-lsp
    src/lsp/main.cpp
    src/lsp/Transport.cpp
    src/lsp/DocumentManager.cpp
    src/lsp/LspServer.cpp
)
target_include_directories(foxlang-lsp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/lsp)
target_link_libraries(foxlang-lsp PRIVATE foxlang_core foxlang_json)

# The old targets remain buildable by downstream projects. Both executable names
# contain exactly the same engine (including the standalone bundle descriptor).
include(GNUInstallDirs)
foreach(pair "foxlang|skrumi" "foxlang-lsp|skrumi-lsp")
    string(REPLACE "|" ";" names "${pair}")
    list(GET names 0 legacy)
    list(GET names 1 modern)
    set_target_properties(${legacy} PROPERTIES OUTPUT_NAME ${modern})
    add_custom_command(TARGET ${legacy} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${legacy}>
                $<TARGET_FILE_DIR:${legacy}>/${legacy}${CMAKE_EXECUTABLE_SUFFIX})
    add_executable(${modern} ALIAS ${legacy})
    install(PROGRAMS $<TARGET_FILE_DIR:${legacy}>/${legacy}${CMAKE_EXECUTABLE_SUFFIX}
            DESTINATION ${CMAKE_INSTALL_BINDIR})
endforeach()

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
        # Functions inlined across files would make every FoxLang call's native frame in
        # the VM loop twice as large, and the recursion a program gets half as deep.
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            set_source_files_properties(src/core/Vm.cpp PROPERTIES COMPILE_OPTIONS "-fno-lto")
        elseif(MSVC)
            set_source_files_properties(src/core/Vm.cpp PROPERTIES COMPILE_OPTIONS "/GL-")
        endif()
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
install(DIRECTORY include/skrumi include/foxlang DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY std DESTINATION ${CMAKE_INSTALL_DATADIR}/skrumi)

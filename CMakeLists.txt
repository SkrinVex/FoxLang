cmake_minimum_required(VERSION 3.16)
project(FoxLang VERSION 5.5.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable compiler warnings
if(MSVC)
    add_compile_options(/W4 /EHsc)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Reusable FoxLang core library
add_library(foxlang_core STATIC
    src/core/Context.cpp
    src/core/Platform.cpp
    src/core/Runtime.cpp
    src/core/Lexer.cpp
    src/core/Parser.cpp
    src/core/Interpreter.cpp
    src/core/SemanticAnalyzer.cpp
)

target_include_directories(foxlang_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# FoxLang CLI executable
add_executable(foxlang src/cli/main.cpp)
target_link_libraries(foxlang PRIVATE foxlang_core)

# FoxLang Language Server executable (LSP)
add_executable(foxlang-lsp
    src/lsp/main.cpp
    src/lsp/Json.cpp
    src/lsp/Transport.cpp
    src/lsp/DocumentManager.cpp
    src/lsp/LspServer.cpp
)
target_include_directories(foxlang-lsp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/lsp)
target_link_libraries(foxlang-lsp PRIVATE foxlang_core)

# CTest integration
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

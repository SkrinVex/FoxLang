# Software rendering needs no OpenGL, GPU driver SDK or separate graphics runtime.
if(WIN32)
    target_link_libraries(foxlang_core PRIVATE user32 gdi32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb)
    if(FOXLANG_STATIC_LINUX)
        # pkg-config's private dependencies (Xau/Xdmcp) are needed for static XCB.
        # Alpine ships libxcb.a but does not ship libXau.a. Build the eight small
        # upstream C files with pinned provenance, rather than adding a runtime DSO.
        include(FetchContent)
        FetchContent_Declare(xau
            URL https://www.x.org/releases/individual/lib/libXau-1.0.12.tar.xz
            URL_HASH SHA256=74d0e4dfa3d39ad8939e99bda37f5967aba528211076828464d2777d477fc0fb)
        FetchContent_MakeAvailable(xau)
        add_library(foxlang_xau STATIC
            ${xau_SOURCE_DIR}/AuDispose.c ${xau_SOURCE_DIR}/AuFileName.c
            ${xau_SOURCE_DIR}/AuGetAddr.c ${xau_SOURCE_DIR}/AuGetBest.c
            ${xau_SOURCE_DIR}/AuLock.c ${xau_SOURCE_DIR}/AuRead.c
            ${xau_SOURCE_DIR}/AuUnlock.c ${xau_SOURCE_DIR}/AuWrite.c)
        target_include_directories(foxlang_xau PRIVATE ${xau_SOURCE_DIR}/include ${XCB_STATIC_INCLUDE_DIRS})
        target_compile_definitions(foxlang_xau PRIVATE HAVE_UNISTD_H=1 HAVE_PATHCONF=1 _DEFAULT_SOURCE=1)
        include(CheckSymbolExists)
        check_symbol_exists(explicit_bzero "string.h" FOXLANG_HAVE_EXPLICIT_BZERO)
        if(FOXLANG_HAVE_EXPLICIT_BZERO)
            target_compile_definitions(foxlang_xau PRIVATE HAVE_EXPLICIT_BZERO=1)
        endif()
        list(TRANSFORM XCB_STATIC_LIBRARIES REPLACE "^Xau$" "foxlang_xau")
        target_include_directories(foxlang_core PRIVATE ${XCB_INCLUDE_DIRS})
        target_link_directories(foxlang_core PRIVATE ${XCB_STATIC_LIBRARY_DIRS})
        target_link_libraries(foxlang_core PRIVATE ${XCB_STATIC_LIBRARIES})
    else()
        target_link_libraries(foxlang_core PRIVATE PkgConfig::XCB)
    endif()
else()
    message(FATAL_ERROR "Native graphics currently supports Linux and Windows")
endif()

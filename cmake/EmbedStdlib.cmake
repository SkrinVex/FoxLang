# Only the small, reviewed standard library is embedded at CMake build time.
# User programs are packaged as data by `foxlang build`, never as generated C++.
file(GLOB std_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/std/*.fox")
set(std_header "#pragma once\n#include <map>\n#include <string>\nnamespace foxlang {\ninline const std::map<std::string, std::string>& embeddedStdlib() {\nstatic const std::map<std::string, std::string> sources = {\n")
foreach(source IN LISTS std_sources)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}")
    get_filename_component(name "${source}" NAME)
    file(READ "${source}" contents HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "\\\\x\\1" contents "${contents}")
    string(APPEND std_header "{\"std/${name}\", \"${contents}\"},\n")
endforeach()
string(APPEND std_header "};\nreturn sources;\n}\n}\n")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedStdlib.h.in" "${std_header}")
configure_file("${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedStdlib.h.in"
               "${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedStdlib.h" COPYONLY)

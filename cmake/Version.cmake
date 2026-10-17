# VERSION is the only file that holds the Skrumi version. Everything else is derived:
# the binaries receive it as SKRUMI_VERSION, and the editor manifests below, which
# must carry a literal version, are rewritten from it on every CMake configure.
#
# Without a build: cmake -P cmake/Version.cmake
if(NOT DEFINED SKRUMI_SOURCE_DIR)
    get_filename_component(SKRUMI_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

file(STRINGS "${SKRUMI_SOURCE_DIR}/VERSION" SKRUMI_VERSION LIMIT_COUNT 1)
string(STRIP "${SKRUMI_VERSION}" SKRUMI_VERSION)
if(NOT SKRUMI_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must contain a major.minor.patch version, got '${SKRUMI_VERSION}'")
endif()

# Replaces the version after the first match of pattern (a regex for the text
# before the version) in a file, writing only when the text changes.
function(skrumi_stamp_version relative pattern)
    set(path "${SKRUMI_SOURCE_DIR}/${relative}")
    file(READ "${path}" old)
    string(REGEX MATCH "${pattern}[0-9]+\\.[0-9]+\\.[0-9]+" found "${old}")
    if(NOT found)
        message(FATAL_ERROR "${relative}: no version field matching '${pattern}'")
    endif()
    string(FIND "${old}" "${found}" at)
    string(REGEX MATCH "^${pattern}" prefix "${found}")
    string(LENGTH "${prefix}" prefix_length)
    string(LENGTH "${found}" found_length)
    math(EXPR version_at "${at} + ${prefix_length}")
    math(EXPR after "${at} + ${found_length}")
    string(SUBSTRING "${old}" 0 ${version_at} before_text)
    string(SUBSTRING "${old}" ${after} -1 after_text)
    set(new "${before_text}${SKRUMI_VERSION}${after_text}")
    if(NOT new STREQUAL old)
        file(WRITE "${path}" "${new}")
        message(STATUS "Skrumi ${SKRUMI_VERSION}: updated ${relative}")
    endif()
endfunction()

skrumi_stamp_version("editors/vscode/package.json" "\n  \"version\": \"")
skrumi_stamp_version("editors/zed/extension.toml" "\nversion = \"")
skrumi_stamp_version("editors/zed/Cargo.toml" "\nversion = \"")
skrumi_stamp_version("editors/zed/Cargo.lock" "name = \"skrumi\"\nversion = \"")

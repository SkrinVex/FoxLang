# Native unit tests
add_executable(test_graphics unit/test_graphics.cpp)
target_link_libraries(test_graphics PRIVATE foxlang_core)
add_test(NAME unit_graphics COMMAND test_graphics)

add_executable(test_lexer unit/test_lexer.cpp)
target_link_libraries(test_lexer PRIVATE foxlang_core)
add_test(NAME unit_lexer COMMAND test_lexer)

add_executable(test_parser unit/test_parser.cpp)
target_link_libraries(test_parser PRIVATE foxlang_core)
add_test(NAME unit_parser COMMAND test_parser)

add_executable(test_interpreter unit/test_interpreter.cpp)
target_link_libraries(test_interpreter PRIVATE foxlang_core)
add_test(NAME unit_interpreter COMMAND test_interpreter)

add_executable(test_certificates unit/test_certificates.cpp)
target_include_directories(test_certificates PRIVATE ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(test_certificates PRIVATE mbedx509)
add_test(NAME unit_certificates COMMAND test_certificates)

add_executable(test_positions unit/test_positions.cpp)
target_link_libraries(test_positions PRIVATE foxlang_core)
add_test(NAME unit_positions COMMAND test_positions)

add_executable(test_semantic_analyzer unit/test_semantic_analyzer.cpp)
target_link_libraries(test_semantic_analyzer PRIVATE foxlang_core)
add_test(NAME unit_semantic_analyzer COMMAND test_semantic_analyzer)

add_executable(test_lsp_protocol
    unit/test_lsp_protocol.cpp
    ${CMAKE_SOURCE_DIR}/src/lsp/Json.cpp
    ${CMAKE_SOURCE_DIR}/src/lsp/Transport.cpp
    ${CMAKE_SOURCE_DIR}/src/lsp/DocumentManager.cpp
    ${CMAKE_SOURCE_DIR}/src/lsp/LspServer.cpp
)
target_include_directories(test_lsp_protocol PRIVATE ${CMAKE_SOURCE_DIR}/src/lsp)
target_link_libraries(test_lsp_protocol PRIVATE foxlang_core)
add_test(NAME unit_lsp_protocol COMMAND test_lsp_protocol)

# Packaging is tested from a copied CLI with no stdlib, then with only the result
# in a clean recipient directory. Python is a test dependency, never a runtime one.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME packaging_versions COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_versions.py ${CMAKE_SOURCE_DIR})
add_executable(test_bundle unit/test_bundle.cpp)
target_link_libraries(test_bundle PRIVATE foxlang_bundle)
add_test(NAME standalone_bundle_format COMMAND test_bundle $<TARGET_FILE:foxlang>)
set_tests_properties(standalone_bundle_format PROPERTIES LABELS standalone)
add_test(NAME standalone_graphics
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/standalone/test_graphics.py $<TARGET_FILE:foxlang>)
set_tests_properties(standalone_graphics PROPERTIES TIMEOUT 90 LABELS standalone SKIP_RETURN_CODE 77)
set(STANDALONE_TESTS hello cli modules unicode_json environment errors corruption http_client http_server tcp)
foreach(T IN LISTS STANDALONE_TESTS)
    add_test(NAME standalone_${T}
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/standalone/test_standalone.py $<TARGET_FILE:foxlang> ${T})
    set_tests_properties(standalone_${T} PROPERTIES TIMEOUT 60 LABELS standalone)
endforeach()

add_test(NAME standalone_https
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/standalone/test_tls.py $<TARGET_FILE:foxlang>)
set_tests_properties(standalone_https PROPERTIES TIMEOUT 90 LABELS standalone)

add_test(NAME standalone_https_server
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/standalone/test_https_server.py $<TARGET_FILE:foxlang>)
set_tests_properties(standalone_https_server PROPERTIES TIMEOUT 90 LABELS standalone)

# Common environment for FoxLang script tests
set(FOXLANG_TEST_ENV
    "FOXLANG_HOME=${CMAKE_SOURCE_DIR}"
    "FOXLANG_TEST_VALUE=works"
    "FOXLANG_LOG_LEVEL=info"
)

# Core smoke tests from original repo
set(SMOKE_TESTS
    core_smoke
    language_core
    control_flow
    stdlib_smoke
    json_unicode
    env_logging
)

foreach(T ${SMOKE_TESTS})
    add_test(NAME smoke_${T} COMMAND foxlang ${CMAKE_CURRENT_SOURCE_DIR}/${T}.fox)
    set_tests_properties(smoke_${T} PROPERTIES ENVIRONMENT "${FOXLANG_TEST_ENV}")
endforeach()

# Comprehensive regression tests
set(REGRESSION_TESTS
    test_syntax_types
    test_expressions
    test_variables_scope
    test_control_flow
    test_functions
    test_arrays
    test_modules
    test_stdlib
    test_json_advanced
    test_files
    test_env
)

foreach(T ${REGRESSION_TESTS})
    add_test(NAME regression_${T} COMMAND foxlang ${CMAKE_CURRENT_SOURCE_DIR}/regression/${T}.fox)
    set_tests_properties(regression_${T} PROPERTIES ENVIRONMENT "${FOXLANG_TEST_ENV}")
endforeach()

# CLI flags tests
add_test(NAME cli_version COMMAND foxlang --version)
string(REPLACE "." "[.]" FOXLANG_VERSION_PATTERN "${PROJECT_VERSION}")
set_tests_properties(cli_version PROPERTIES PASS_REGULAR_EXPRESSION "FoxLang ${FOXLANG_VERSION_PATTERN}")

add_test(NAME cli_help COMMAND foxlang --help)
set_tests_properties(cli_help PROPERTIES PASS_REGULAR_EXPRESSION "https://github.com/SkrinVex/FoxLang")

add_test(NAME lsp_cli_version COMMAND foxlang-lsp --version)
set_tests_properties(lsp_cli_version PROPERTIES PASS_REGULAR_EXPRESSION "foxlang-lsp ${FOXLANG_VERSION_PATTERN}")

add_test(NAME lsp_cli_help COMMAND foxlang-lsp --help)
set_tests_properties(lsp_cli_help PROPERTIES PASS_REGULAR_EXPRESSION "Language Server")

# HTTP server/client local integration test (POSIX)
if(NOT WIN32)
    find_program(BASH_EXECUTABLE bash)
    if(BASH_EXECUTABLE)
        add_test(NAME integration_http_local
            COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/regression/test_http_local.sh $<TARGET_FILE:foxlang>
        )
        set_tests_properties(integration_http_local PROPERTIES
            ENVIRONMENT "${FOXLANG_TEST_ENV}"
            TIMEOUT 20
        )

        add_test(NAME integration_logging_levels
            COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/regression/test_logging_levels.sh $<TARGET_FILE:foxlang>
        )
        set_tests_properties(integration_logging_levels PROPERTIES
            ENVIRONMENT "${FOXLANG_TEST_ENV}"
        )

        add_test(NAME integration_error_handling
            COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/regression/test_error_handling.sh $<TARGET_FILE:foxlang>
        )
        set_tests_properties(integration_error_handling PROPERTIES
            ENVIRONMENT "${FOXLANG_TEST_ENV}"
        )
    endif()
endif()

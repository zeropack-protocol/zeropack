cmake_minimum_required(VERSION 3.15)
project(zeropack
    VERSION 0.1.0
    DESCRIPTION "Zero-heap, zero-copy, freestanding C11 serialization protocol"
    LANGUAGES C
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ZEROPACK_BUILD_TESTS "Build zeropack unit tests" OFF)
option(ZEROPACK_BUILD_EXAMPLES "Build zeropack examples" OFF)

add_library(zeropack STATIC
    src/core.c
    src/tx.c
    src/rx.c
    src/handshake.c
    src/pool.c
    src/types.c
)

add_library(zeropack::zeropack ALIAS zeropack)

target_compile_features(zeropack PUBLIC c_std_11)

target_include_directories(zeropack PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(MSVC)
    target_compile_options(zeropack PRIVATE /W4)
else()
    target_compile_options(zeropack PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(ZEROPACK_BUILD_EXAMPLES)
    add_executable(example_minimal examples/minimal.c)
    target_link_libraries(example_minimal PRIVATE zeropack)
    target_include_directories(example_minimal PRIVATE examples)
    set_target_properties(example_minimal PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
endif()

if(ZEROPACK_BUILD_TESTS)
    enable_testing()

    add_executable(test_runner test/test_main.c)
    target_link_libraries(test_runner PRIVATE zeropack)
    target_include_directories(test_runner PRIVATE test)
    set_target_properties(test_runner PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
    add_test(NAME zeropack_unit_tests COMMAND test_runner)

    if(MSVC)
        set(FAIL_WARN_FLAG "/WX")
        set(FAIL_OUT_FLAG "/Fo:NUL")
    else()
        set(FAIL_WARN_FLAG "-Werror")
        set(FAIL_OUT_FLAG "-o" "/dev/null")
    endif()

    add_test(
        NAME negative_duplicate_action
        COMMAND ${CMAKE_C_COMPILER}
            -I${CMAKE_CURRENT_SOURCE_DIR}/include
            ${FAIL_WARN_FLAG}
            -DFAIL_DUPLICATE_ACTION
            -c ${CMAKE_CURRENT_SOURCE_DIR}/test/test_fail.c
            ${FAIL_OUT_FLAG}
    )
    set_tests_properties(negative_duplicate_action PROPERTIES
        PASS_REGULAR_EXPRESSION "redeclaration|redefinition|duplicate"
    )

    add_test(
        NAME negative_undeclared_type
        COMMAND ${CMAKE_C_COMPILER}
            -I${CMAKE_CURRENT_SOURCE_DIR}/include
            ${FAIL_WARN_FLAG}
            -DFAIL_UNDECLARED_TYPE
            -c ${CMAKE_CURRENT_SOURCE_DIR}/test/test_fail.c
            ${FAIL_OUT_FLAG}
    )
    set_tests_properties(negative_undeclared_type PROPERTIES
        PASS_REGULAR_EXPRESSION "undeclared"
    )
endif()

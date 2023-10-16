project(error_tests)

add_executable(
    test_error
)

target_sources(
    test_error
    PRIVATE test_Error.cc
)

target_link_libraries(
    test_error
    PRIVATE avctool::error
            avctool::log
)
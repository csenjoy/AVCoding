project(logtest)

add_executable(test_format)

target_sources(test_format
    PRIVATE test_formatio.cc
)

add_executable(test_logger)

target_sources(test_logger
   PRIVATE test_logger.cc
)

target_link_libraries(
    test_logger
    PRIVATE log
)
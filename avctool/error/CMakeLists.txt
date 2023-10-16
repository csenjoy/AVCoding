project(errno)

file(GLOB ERRNO_HEADERS "*.h")
file(GLOB ERRNO_SOURCES "*.cc")

add_library(error)
add_library(avctool::error ALIAS error)

target_sources(
    error
    PRIVATE ${ERRNO_HEADERS}
            ${ERRNO_SOURCES}
)       

target_include_directories(
    error
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../
)

add_subdirectory(test)
project(thread)

add_library(thread)
add_library(avctool::thread ALIAS thread)

file(GLOB THREAD_HEADERS "*.h")
file(GLOB THREAD_SOURCES "*.cc")

target_sources(
    thread
    PRIVATE ${THREAD_HEADERS}
            ${THREAD_SOURCES}
)

target_include_directories(
    thread
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../
)

source_group("" FILES ${THREAD_HEADERS} ${THREAD_SOURCES})
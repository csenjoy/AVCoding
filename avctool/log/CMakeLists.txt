project(log)

add_library(log)
add_library(avctool::log ALIAS log)

file(GLOB LOG_HEADERS "*.h")
file(GLOB LOG_SOURCES "*.cc")


target_sources(log
    PRIVATE ${LOG_HEADERS}
            ${LOG_SOURCES}
)

target_link_libraries(
    log
    PUBLIC avctool::util
)

source_group("" FILES ${LOG_HEADERS} ${LOG_SOURCES})
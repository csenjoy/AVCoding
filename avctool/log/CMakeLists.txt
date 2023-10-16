project(log)

add_subdirectory(test)

add_library(log)
add_library(avctool::log ALIAS log)

target_sources(log
    PRIVATE Log.h
            Log.cc
)

target_link_libraries(
    log
    PUBLIC util
)

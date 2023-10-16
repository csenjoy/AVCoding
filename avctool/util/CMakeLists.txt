project(util)

add_library(util)
add_library(avctool::util ALIAS util)

target_sources(
    util
    PRIVATE Nocopyable.h
            Semphore.h
            Util.h
            Util.cc
)

target_include_directories(
    util
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../
)
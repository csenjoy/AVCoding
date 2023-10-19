project(util)

add_library(util)
add_library(avctool::util ALIAS util)

file(GLOB UTIL_HEADERS "*.h")
file(GLOB UTIL_SOURCES "*.cc")

target_sources(
    util
    PRIVATE ${UTIL_HEADERS}
            ${UTIL_SOURCES}
)

target_include_directories(
    util
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../
)

#util库并不依赖这些库，链接的原因是导出这些库
#其他项目仅需要链接util库即可
#target_link_libraries(
#    util
#    PUBLIC avctool::log
#           avctool::error
#           avctool::thread
#           avctool::network
#           avctool::poller   
#)

source_group("" FILES ${UTIL_HEADERS} ${UTIL_SOURCES})
#add_subdirectory(test)
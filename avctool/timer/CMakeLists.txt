project(timer)

add_library(timer)
add_library(avctool::timer ALIAS timer)

file(GLOB timer_HEADERS "*.h")
file(GLOB timer_SOURCES "*.cc")

target_sources(
    timer
    PRIVATE ${timer_HEADERS}
            ${timer_SOURCES}
)       

target_link_libraries(
    timer
    PUBLIC avctool::network
)

target_include_directories(
    timer
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdpart
)

source_group("" FILES ${timer_HEADERS} ${timer_SOURCES}) 
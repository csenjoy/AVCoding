project(poller)

add_library(poller)
add_library(avctool::poller ALIAS poller)

file(GLOB POLLER_HEADERS "*.h")
file(GLOB POLLER_SOURCES "*.cc")


if (WIN32)
    list(APPEND POLLER_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/../../3rdpart/wepoll/wepoll.h")
    list(APPEND POLLER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../../3rdpart/wepoll/wepoll.c")
endif(WIN32)

target_sources(
    poller
    PRIVATE ${POLLER_HEADERS}
            ${POLLER_SOURCES}
)       

target_link_libraries(
    poller
    PUBLIC avctool::network
)

target_include_directories(
    poller
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdpart
)

source_group("" FILES ${POLLER_HEADERS} ${POLLER_SOURCES}) 
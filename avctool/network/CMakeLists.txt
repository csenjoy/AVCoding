project(network)

add_library(network)
add_library(avctool::network ALIAS network)

file(GLOB NETWORK_HEADERS "*.h")
file(GLOB NETWORK_SOURCES "*.cc")

target_sources(
    network
    PRIVATE ${NETWORK_HEADERS}
            ${NETWORK_SOURCES}
)       

target_link_libraries(
    network
    PUBLIC avctool::util
           avctool::log
           avctool::error
)

source_group("" FILES ${NETWORK_HEADERS} ${NETWORK_SOURCES}) 
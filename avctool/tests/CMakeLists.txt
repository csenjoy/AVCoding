project(avctool_tests)

#获取当前目录下所有源文件，对每个源文件创建一个executable
file(GLOB AVCTOOL_TESTS "*.cc")

foreach(test ${AVCTOOL_TESTS})
    #获取路径的文件名(NAME_WE不包含后缀名)
    get_filename_component(test_proj ${test} NAME_WE)
    #创建可执行程序
    add_executable(${test_proj} ${test})
    set_property(TARGET ${test_proj} PROPERTY FOLDER "tests")
    source_group("" FILES ${test})

    target_link_libraries(${test_proj} 
        PRIVATE avctool::util
                avctool::log
                avctool::error
                avctool::network
                avctool::poller
    )
endforeach(test ${AVCTOOL_TESTS})

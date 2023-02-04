message("\n------------------ Start generate : Test ------------------")

aux_source_directory(. TEST_SRC_LIST)

foreach(TEST_SRC ${TEST_SRC_LIST})
    STRING(REGEX REPLACE "^\\./|\\.c[a-zA-Z0-9_]*$" "" TEST_EXE_NAME ${TEST_SRC})
    message(STATUS "添加测试程序:${TEST_EXE_NAME}")
    
    add_executable(${TEST_EXE_NAME} ${TEST_SRC})
    target_include_directories(${TEST_EXE_NAME} 
        PRIVATE
            ../src
        )

    message("CMAKE_ARCHIVE_OUTPUT_DIRECTORY: " ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
    target_link_directories(${TEST_EXE_NAME}
        PRIVATE
            ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
        )

    target_link_libraries(${TEST_EXE_NAME} 
        PRIVATE 
            ${PROJECT_NAME}
        )

endforeach()

message("------------------ Stop generate : Test ------------------\n")

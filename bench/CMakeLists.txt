add_executable(bench bench.cpp)

target_include_directories(bench PRIVATE ../src)

target_link_directories(bench PRIVATE ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

# target_compile_options(bench PRIVATE -Wno-unused-function)

target_link_libraries(bench ${PROJECT_NAME} spdlog::spdlog)

# if(IS_BUILD_INSTALL)
#     InstallFunc(${PROJECT_NAME})
# endif() 
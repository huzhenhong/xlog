
include(FetchContent)
FetchContent_Declare(spdlog
        # GIT_REPOSITORY git@github.com:gabime/spdlog.git
        # GIT_REPOSITORY https://github.com/gabime/spdlog.git
        # GIT_TAG ad0e89cbfb4d0c1ce4d097e134eb7be67baebb36
        # GIT_TAG v1.11.0
        # GIT_SHALLOW    TRUE # 不拉取完整历史，相当于`git clone --depth=1`


        URL      https://github.com/gabime/spdlog/archive/refs/tags/v1.11.0.tar.gz
        # URL_HASH MD5=287c6492c25044fd2da9947ab120b2bd
        # HASH 287c6492c25044fd2da9947ab120b2bd # 可选，确保文件的正确性

        SOURCE_DIR ${CMAKE_SOURCE_DIR}/ext/spdlog
        SUBBUILD_DIR ${CMAKE_SOURCE_DIR}/ext/build
        BINARY_DIR ${CMAKE_SOURCE_DIR}/ext/bin
        )
FetchContent_MakeAvailable(spdlog)

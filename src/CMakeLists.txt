
set(Target ${PROJECT_NAME})

message("\n------------------ Start generate : ${Target} ------------------")

# aux_source_directory(. SRCS)
aux_source_directory(fmt fmt_SRCS)
FILE(GLOB_RECURSE SRCS CONFIGURE_DEPENDS *.h *.c *.hpp *.cpp)

# message("src: ${SRCS}")
# message("fmt_SRCS: ${fmt_SRCS}")

# foreach(src ${SRCS})
#     message("src: ${SRCS}")
# endforeach()

set(${Target}_SRCS 
        ${SRCS}
        ${fmt_SRCS}
        )

# add_library(${Target} STATIC ${${Target}_SRCS})
add_library(${Target} SHARED ${${Target}_SRCS})


# 开启导出动态库
target_compile_definitions(${Target}
    PRIVATE
        LIBRARY_EXPORT
)

# 导出接口头文件
set(${Target}_EXPORT_HEADER 
    ${CMAKE_CURRENT_SOURCE_DIR}/common/SysInclude.h
    ${CMAKE_CURRENT_SOURCE_DIR}/common/ExportMarco.h
)

set_target_properties(${Target} 
    PROPERTIES 
        PUBLIC_HEADER 
            "${${Target}_EXPORT_HEADER}"
    )

message("------------------ Stop generate : ${Target} ------------------\n")

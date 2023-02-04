
# # opencv相关
# find_package(OpenCV REQUIRED)
# if(NOT OpenCV_FOUND)
#     message(FATAL_ERROR "OpenCV not found!")
# endif(NOT OpenCV_FOUND)

# include_directories(${OpenCV_INCLUDE_DIRS})
# link_directories(${OpenCV_DIR})
# set(OPENCV_NEED_LIBS opencv_core opencv_imgproc opencv_dnn)

# message("opencv include dir : " ${OpenCV_INCLUDE_DIRS})
# message("opencv lib dir : " ${OpenCV_LIBRARY_DIRS})
# message("opencv lib : " ${OPENCV_NEED_LIBS})

# find_package(CUDA REQUIRED)
# if(NOT CUDA_FOUND)
#     message(ERROR "CUDA not found!")
# endif(NOT CUDA_FOUND)

# include_directories(${CUDA_INCLUDE_DIRS})
# link_directories(${CUDA_LIBRARY_DIRS})
# set(TRT_NEED_LIBS nvinfer nvinfer_plugin nvparsers nvonnxparser cudart)
# # set(TRT_NEED_LIBS nvinfer nvinfer_plugin nvparsers nvonnxparser myelin64_1 cudart)

# message("CUDA include dir : " ${CUDA_INCLUDE_DIRS}) 
# message("CUDA lib dir : " ${CUDA_LIBRARY_DIRS})
# message("CUDA lib dir : " ${TRT_NEED_LIBS})

# if(CUDA_ENABLE)
#     enable_language(CUDA)
# endif()

# # tensorrt
# set(TensorRT_DIR D://DevEnv/TensorRT-7.1.3.4.Windows10.x86_64.cuda-11.0.cudnn8.0/TensorRT-7.1.3.4)
# if(DEFINED TensorRT_DIR)
#   message("TensorRT include : " ${TensorRT_DIR}/include)
#   include_directories(${TensorRT_DIR}/include)
#   link_directories(${TensorRT_DIR}/lib)
# endif(DEFINED TensorRT_DIR)

cmake_minimum_required(VERSION 3.30)
project(GeospatialDataSystem)

set(CMAKE_CXX_STANDARD 17)

# 配置最终的可执行文件输出路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 项目打包库输出路径
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY  ${PROJECT_SOURCE_DIR}/lib)

# 阿里云服务器Python配置
#set(Python3_INCLUDE_DIR "/usr/include/python3.12")
#set(Python3_LIBRARY "/usr/lib/x86_64-linux-gnu/libpython3.12.so")

## 本地虚拟机Python配置
set(Python3_INCLUDE_DIR "/usr/include/python3.8")
set(Python3_LIBRARY "/usr/lib/x86_64-linux-gnu/libpython3.8.so")




include_directories(include)

add_subdirectory(src)
add_subdirectory(test)


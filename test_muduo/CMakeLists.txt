cmake_minimum_required(VERSION 3.0)
project(test_muduo)

# 配置编译选项
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -g)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

set(SRC_LIST ${PROJECT_SOURCE_DIR}/muduo_server.cpp)
add_executable(server ${SRC_LIST})
target_link_libraries(server muduo_net muduo_base pthread)
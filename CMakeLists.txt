cmake_minimum_required(VERSION 3.10)
project(OneTaskCore VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准 [cite: 64]
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 自动搜寻头文件路径
include_directories(include)

# --- 1. 编译核心静态库 ---
# 包含所有源文件：监控器、调度器、线程池
add_library(OneTask STATIC 
    src/monitor/monitor.cpp
    src/scheduler/scheduler.cpp
    src/threadpool/thread_pool.cpp 
)

# 确保链接了 pthread 库（多线程核心） 
find_package(Threads REQUIRED)
target_link_libraries(OneTask PUBLIC Threads::Threads)

# --- 2. 编译测试 Demo ---
add_executable(onetask_demo demo/simpledemo.cpp)

# 将 Demo 链接到我们的核心库上 
target_link_libraries(onetask_demo PRIVATE OneTask)

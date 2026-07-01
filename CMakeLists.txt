cmake_minimum_required(VERSION 3.10)
project(thread_pool CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra)

find_package(Threads REQUIRED)

add_executable(thread-pool-demo src/main.cpp)
target_include_directories(thread-pool-demo PRIVATE include)
target_link_libraries(thread-pool-demo PRIVATE Threads::Threads)

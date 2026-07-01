cmake_minimum_required(VERSION 3.10)
project(thread_pool CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra)

# The thread pool needs pthreads on Linux for std::thread to actually work.
find_package(Threads REQUIRED)

add_executable(thread-pool-demo src/main.cpp)
target_include_directories(thread-pool-demo PRIVATE include)
target_link_libraries(thread-pool-demo PRIVATE Threads::Threads)

# Debug builds get ThreadSanitizer for free -- catches race conditions at
# runtime that are otherwise nearly impossible to find by just reading code.
# Note: ThreadSanitizer and AddressSanitizer can't be combined in one binary,
# which is why this only enables TSan (the more important one for this project).
target_compile_options(thread-pool-demo PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=thread -g>
)
target_link_options(thread-pool-demo PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=thread>
)

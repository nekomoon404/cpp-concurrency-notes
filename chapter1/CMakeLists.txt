cmake_minimum_required(VERSION 3.10)

# set the project name
project(ThreadProject)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# add the executable
add_executable(ThreadProject thread_base.cc)

# link the threads library
find_package(Threads REQUIRED)

# print linked thread library path
# get_target_property(THREAD_LIB_PATH Threads::Threads LOCATION)
# message(STATUS "Threads library path: ${THREAD_LIB_PATH}")

target_link_libraries(ThreadProject Threads::Threads)


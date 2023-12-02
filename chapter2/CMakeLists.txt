cmake_minimum_required(VERSION 3.10)

# set the project name
project(ThreadProject)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# add the executable
add_executable(ThreadProject thread_manage.cc)

# link the threads library
find_package(Threads REQUIRED)
target_link_libraries(ThreadProject Threads::Threads)


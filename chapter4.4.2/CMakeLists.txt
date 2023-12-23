cmake_minimum_required(VERSION 3.10)

# set the project name
project(ThreadProject)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(${CMAKE_SOURCE_DIR}/include)

set(SRC_LIST "")
set(SRC_LIST_TEMP "")
file(GLOB SRC_LIST_TEMP
    ${CMAKE_SOURCE_DIR}/src/*.h
    ${CMAKE_SOURCE_DIR}/src/*.c
    ${CMAKE_SOURCE_DIR}/src/*.cc)
# source_group(src FILES ${SRC_LIST_TEMP})
set(SRC_LIST ${SRC_LIST} ${SRC_LIST_TEMP})

# add the executable
add_executable(ThreadProject main.cc ${SRC_LIST})

# link the threads library
find_package(Threads REQUIRED)
target_link_libraries(ThreadProject Threads::Threads)
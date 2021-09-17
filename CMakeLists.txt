cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

project(isearch CXX C)

# enable c++ 11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# take respectives flags for debug & release process 
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-std=c++11 -g -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-std=c++11 -g -O2")

# default build type : Debug
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_subdirectory(src/comm)
add_subdirectory(src/comm/stat)
#add_subdirectory(src/search_local/index_write)
#add_subdirectory(src/search_local/index_read)
add_subdirectory(src/search_agent)
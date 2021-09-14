cmake_minimum_required(VERSION 2.6)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O2 -lrt -Wall")
AUX_SOURCE_DIRECTORY(. main)
AUX_SOURCE_DIRECTORY(./segment segment)
add_library(common ${main} ${segment})
target_include_directories(common  PUBLIC
./
../search_local/index_storage/api/c_api_cc
../3rdlib/jsoncpp/include
./stat
)

cmake_minimum_required(VERSION 2.6)

AUX_SOURCE_DIRECTORY(. main)
add_library(common ${main})
target_include_directories(common  PUBLIC
./
../search_local/index_storage/api/c_api_cc
../3rdlib/jsoncpp/include
./stat
)

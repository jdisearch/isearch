cmake_minimum_required(VERSION 2.6)

PROJECT(index_read)
EXECUTE_PROCESS(COMMAND git log -1 --pretty=format:%h . OUTPUT_VARIABLE version)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -g -O2 -lrt -Wall")

AUX_SOURCE_DIRECTORY(. main)
AUX_SOURCE_DIRECTORY(./index_sync index_sync)
AUX_SOURCE_DIRECTORY(./utils utils)
AUX_SOURCE_DIRECTORY(./correction correction)
AUX_SOURCE_DIRECTORY(./query query)
AUX_SOURCE_DIRECTORY(./process process)
AUX_SOURCE_DIRECTORY(./sort_operator sort_operator)

LINK_DIRECTORIES(   
${PROJECT_SOURCE_DIR}/../../comm
${PROJECT_SOURCE_DIR}/../../3rdlib/jsoncpp/lib
${PROJECT_SOURCE_DIR}/../../comm/stat
${PROJECT_SOURCE_DIR}/../index_storage/api/c_api_cc
)

ADD_EXECUTABLE(index_read ${main} ${index_sync} ${utils} ${correction} ${query} ${process} ${sort_operator})

target_include_directories(index_read  PUBLIC
../../3rdlib/jsoncpp/include
../../comm
../../comm/stat
../../comm/segment
../index_storage/api/c_api_cc
)

add_definitions(-DACTIVE_GIT_VERSION="${version}" -DMAIN)

target_link_libraries(index_read  libcommon.a libdtc.so jsoncpp stat ssl pthread dl)
SET_TARGET_PROPERTIES(index_read  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "./bin")



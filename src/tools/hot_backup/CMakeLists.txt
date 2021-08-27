cmake_minimum_required(VERSION 2.6)

PROJECT(hotbackup)
EXECUTE_PROCESS(COMMAND git log -1 --pretty=format:%h . OUTPUT_VARIABLE version)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11 -g  -lrt -Wall")

AUX_SOURCE_DIRECTORY(. main)

LINK_DIRECTORIES(
${PROJECT_SOURCE_DIR}/../../comm
${PROJECT_SOURCE_DIR}/../../3rdlib/jsoncpp/lib
${PROJECT_SOURCE_DIR}/../../comm/stat
${PROJECT_SOURCE_DIR}/../../search_local/index_storage/common
)

ADD_EXECUTABLE(hotbackup   ${main})

target_include_directories(hotbackup  PUBLIC
../../3rdlib/jsoncpp/include
../../comm
../../comm/stat
../../search_local/index_storage/common
../../search_local/index_storage/api/c_api_cc
)

add_definitions(-DACTIVE_GIT_VERSION="${version}" -DMAIN)

target_link_libraries(hotbackup  libcommon.a libcommon_nopic.a libdtc.so jsoncpp stat ssl pthread)
SET_TARGET_PROPERTIES(hotbackup  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "./bin")

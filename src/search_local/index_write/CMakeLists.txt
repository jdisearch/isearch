cmake_minimum_required(VERSION 2.6)

PROJECT(index_write)
EXECUTE_PROCESS(COMMAND git log -1 --pretty=format:%h . OUTPUT_VARIABLE version)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g  -lrt -Wall")

AUX_SOURCE_DIRECTORY(. main)

LINK_DIRECTORIES(
${PROJECT_SOURCE_DIR}/../../comm
${PROJECT_SOURCE_DIR}/../../3rdlib/jsoncpp/lib
${PROJECT_SOURCE_DIR}/../../comm/stat
)

ADD_EXECUTABLE(index_write   ${main})

target_include_directories(index_write  PUBLIC
../../3rdlib/jsoncpp/include
../../comm
../../comm/stat
../index_storage/api/c_api_cc
)

add_definitions(-DGIT_VERSION="${version}" -DMAIN)

target_link_libraries(index_write  libcommon.a  libdtc.so jsoncpp stat ssl)
SET_TARGET_PROPERTIES(index_write  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "./bin")

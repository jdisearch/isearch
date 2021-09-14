cmake_minimum_required(VERSION 2.6)

PROJECT(correction_unittest)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fprofile-arcs -ftest-coverage -lrt -Wall -O2")

AUX_SOURCE_DIRECTORY(. main)
set(OTHER_FILE "../correction/correction.cc" "../utils/get_aois_action.cc"
 "../search_conf.cc" "../split_manager.cc" "../data_manager.cc" "../search_util.cc"
 "../db_manager.cc" "../request_context.cc" "../result_context.cc")

LINK_DIRECTORIES(   
${PROJECT_SOURCE_DIR}/../../../comm
${PROJECT_SOURCE_DIR}/../../../comm/stat
${PROJECT_SOURCE_DIR}/../../../3rdlib/jsoncpp/lib
${PROJECT_SOURCE_DIR}/../../../3rdlib/google_test/lib
)

ADD_EXECUTABLE(correction_unittest ${main} ${OTHER_FILE})

target_include_directories(correction_unittest  PUBLIC
../
../utils
../correction
../../../comm
../../../comm/stat
../../../comm/segment
../../../3rdlib/google_test/include
../../../3rdlib/jsoncpp/include
../../../search_local/index_storage/api/c_api_cc
)

target_link_libraries(correction_unittest common gtest_main gtest jsoncpp stat dl -lpthread -lm)
SET_TARGET_PROPERTIES(correction_unittest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "./bin")



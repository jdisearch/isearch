cmake_minimum_required(VERSION 2.6)

PROJECT(search_agent)
EXECUTE_PROCESS(COMMAND git log -1 --pretty=format:%h . OUTPUT_VARIABLE version)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g  -lrt -Wall")

AUX_SOURCE_DIRECTORY(. main)

LINK_DIRECTORIES(
${PROJECT_SOURCE_DIR}/../comm
${PROJECT_SOURCE_DIR}/../3rdlib/jsoncpp/lib
)

ADD_EXECUTABLE(search_agent  ${main})

target_include_directories(search_agent  PUBLIC
../3rdlib/jsoncpp/include
../comm
)

add_definitions(-DGIT_VERSION="${version}" -DMAIN)

target_link_libraries(search_agent  libcommon.a  jsoncpp ssl)
SET_TARGET_PROPERTIES(search_agent  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "./bin")

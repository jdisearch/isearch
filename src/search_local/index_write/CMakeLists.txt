option(jdtestOpen "Build gtest's units programs." OFF)
option(INDEX_WRITE_OUTPUT "Build index_write programs." ON)

cmake_minimum_required(VERSION 2.6)

PROJECT(index_write_unit)
EXECUTE_PROCESS(COMMAND git log -1 --pretty=format:%h . OUTPUT_VARIABLE version)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -fprofile-arcs -ftest-coverage  -g  -lrt -Wall")

AUX_SOURCE_DIRECTORY(. main)

LINK_DIRECTORIES(
${PROJECT_SOURCE_DIR}/../../comm
${PROJECT_SOURCE_DIR}/../../3rdlib/jsoncpp/lib
${PROJECT_SOURCE_DIR}/../../3rdlib/google_test/lib
${PROJECT_SOURCE_DIR}/../../comm/stat
${PROJECT_SOURCE_DIR}/../index_storage/api/c_api_cc
)


INCLUDE_DIRECTORIES(
${PROJECT_SOURCE_DIR}/../../3rdlib/jsoncpp/include
${PROJECT_SOURCE_DIR}/../../3rdlib/google_test/include 
${PROJECT_SOURCE_DIR}/../../comm
${PROJECT_SOURCE_DIR}/../../comm/stat
${PROJECT_SOURCE_DIR}/../index_storage/api/c_api_cc
)


if(INDEX_WRITE_OUTPUT)
ADD_EXECUTABLE(index_write   ${main})
add_definitions(-DGIT_VERSION="${version}" -DMAIN)
target_link_libraries(index_write pthread libcommon.a  libdtc.so jsoncpp stat ssl)
SET_TARGET_PROPERTIES(index_write  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "./bin")
endif()

if(jdtestOpen)
	cmake_minimum_required(VERSION 2.8.8)

	if (POLICY CMP0048)
		cmake_policy(SET CMP0048 NEW)
	endif (POLICY CMP0048)
	
	if (CMAKE_VERSION VERSION_LESS "3.1")
		add_definitions(-std=c++11)
	else()
		set(CMAKE_CXX_STANDARD 11)
		set(CMAKE_CXX_STANDARD_REQUIRED ON)
		if(NOT CYGWIN)
			set(CMAKE_CXX_EXTENSIONS OFF)
		endif()
	endif()
	
	list(REMOVE_ITEM main "./main.cc")
	AUX_SOURCE_DIRECTORY(./jdtest jdtestFiles)
	
	ADD_EXECUTABLE(gtest_index_write ${main} ${jdtestFiles})
	TARGET_LINK_LIBRARIES(gtest_index_write gmock gtest pthread libcommon.a libdtc.so jsoncpp stat ssl)
	SET_TARGET_PROPERTIES(gtest_index_write  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "./bin")
	install(TARGETS gtest_index_write RUNTIME DESTINATION bin)
endif()

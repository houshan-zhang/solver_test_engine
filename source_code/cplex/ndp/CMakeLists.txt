# CMake version limit
cmake_minimum_required(VERSION 2.8)

# project name
project("cplex_callback_ndp")

# include path
include_directories(${PROJECT_SOURCE_DIR}/../../../ThirdParty/cplex/include)

# lib path
link_directories(${PROJECT_SOURCE_DIR}/../../../ThirdParty/cplex/lib)

# executable file path
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/../../../check/bin)

# specify the version, Release or Debug
if( CMAKE_BUILD_TYPE STREQUAL "Debug" )
   add_definitions(-DDEBUG)
else()
   set(CMAKE_BUILD_TYPE "Release")
   add_definitions(-DNDEBUG)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -std=c++11")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall -std=c++11")

# find all source files in the ./src directory
aux_source_directory(./src DIR_SRCS)

# executable file name
add_executable(cplex_callback_ndp ${DIR_SRCS})

# link cplex libraries
target_link_libraries(cplex_callback_ndp m)
target_link_libraries(cplex_callback_ndp cplex)
target_link_libraries(cplex_callback_ndp pthread)
target_link_libraries(cplex_callback_ndp dl)

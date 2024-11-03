cmake_minimum_required(VERSION 3.11 FATAL_ERROR)
project(in4310 CXX)

set(MAIN_EXE_NAME "Watercolour")

# Binaries directly to the binary dir without subfolders.
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/framework")
	# Create framework library and include CMake scripts (compiler warnings, sanitizers and static analyzers).
	add_subdirectory("framework") 
else() 
	# During development the framework lives in parent folder.
	add_subdirectory("../../../framework/" "${CMAKE_BINARY_DIR}/framework/")
endif()

add_executable(${MAIN_EXE_NAME} "src/main.cpp")

target_compile_features(${MAIN_EXE_NAME} PRIVATE cxx_std_20)
target_link_libraries(${MAIN_EXE_NAME} PRIVATE CGFramework)
enable_sanitizers(${MAIN_EXE_NAME})
set_project_warnings(${MAIN_EXE_NAME})

# OpenMP support.
find_package(OpenMP)
if(OpenMP_CXX_FOUND) 
    target_link_libraries(${MAIN_EXE_NAME} PRIVATE OpenMP::OpenMP_CXX)
endif()

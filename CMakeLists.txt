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

# SET cwd for the MSVS debugger: https://stackoverflow.com/questions/41864259/how-to-set-working-directory-for-visual-studio-2017-rc-cmake-project
# set (VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}) 
set_target_properties(
    ${MAIN_EXE_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# # Copy all files in the resources folder to the build directory after every successful build.
# add_custom_command(TARGET ${MAIN_EXE_NAME} POST_BUILD
# 	COMMAND ${CMAKE_COMMAND} -E copy_directory
# 	"${CMAKE_CURRENT_LIST_DIR}/data" $<TARGET_FILE_DIR:${MAIN_EXE_NAME}>/data)
 
# Preprocessor definitions for path.
target_compile_definitions(${MAIN_EXE_NAME} PRIVATE "-DMODELS_DIR=\"${CMAKE_CURRENT_LIST_DIR}/models/\"")

set(shader_copies "")
# List all *.glsl files in the shaders folder
file(GLOB shader_sources "${CMAKE_CURRENT_LIST_DIR}/shaders/*.glsl")
foreach (shader_file IN LISTS shader_sources)
	get_filename_component(file_name ${shader_file} NAME)
	message("shader_file: ${file_name}")
	add_custom_command(
		OUTPUT "${CMAKE_BINARY_DIR}/shaders/${file_name}"
		COMMAND ${CMAKE_COMMAND} -E copy
		"${CMAKE_CURRENT_LIST_DIR}/shaders/${file_name}"
		"${CMAKE_BINARY_DIR}/shaders/${file_name}"
		DEPENDS "${CMAKE_CURRENT_LIST_DIR}/shaders/${file_name}"
		)
	LIST(APPEND shader_copies "${CMAKE_BINARY_DIR}/shaders/${file_name}")
endforeach()
add_custom_target(copy_shaders DEPENDS ${shader_copies})
add_dependencies(${MAIN_EXE_NAME} copy_shaders)

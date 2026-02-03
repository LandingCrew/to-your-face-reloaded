cmake_minimum_required(VERSION 3.22)

# Version can be overridden via -DPROJECT_VERSION=x.y.z
if(NOT DEFINED PROJECT_VERSION)
	set(PROJECT_VERSION "1.0.0")
endif()

project(
	ToYourFaceReloaded
	VERSION ${PROJECT_VERSION}
	LANGUAGES CXX
)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
	message(FATAL_ERROR "in-source builds are not allowed")
endif()

macro(set_from_environment VARIABLE)
	if(NOT DEFINED ${VARIABLE} AND DEFINED ENV{${VARIABLE}})
		set(${VARIABLE} $ENV{${VARIABLE}})
	endif()
endmacro()

set_from_environment(CompiledPluginsPath)
if(NOT DEFINED CompiledPluginsPath)
	message(FATAL_ERROR "CompiledPluginsPath is not set. Set environment variable: $env:CompiledPluginsPath = \"path\"")
endif()

option(COPY_OUTPUT "Copy the output of build operations to the game directory" ON)
option(ENABLE_SKYRIM_SE "Enable support for Skyrim SE in the dynamic runtime feature." ON)
option(ENABLE_SKYRIM_AE "Enable support for Skyrim AE in the dynamic runtime feature." ON)
option(ENABLE_SKYRIM_VR "Enable support for Skyrim VR in the dynamic runtime feature." OFF)
set(BUILD_TESTS OFF)

# Get git commit hash (short form)
execute_process(
	COMMAND git rev-parse --short HEAD
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	OUTPUT_VARIABLE GIT_COMMIT_HASH
	OUTPUT_STRIP_TRAILING_WHITESPACE
	ERROR_QUIET
)
if(NOT GIT_COMMIT_HASH)
	set(GIT_COMMIT_HASH "unknown")
endif()
message(STATUS "Git commit: ${GIT_COMMIT_HASH}")
message(STATUS "Project version: ${PROJECT_VERSION}")
message(STATUS "Output directory: ${CompiledPluginsPath}")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

add_subdirectory(src)

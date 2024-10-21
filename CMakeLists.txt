# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ESPRelayBoard)

# Set the target
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")

# SPIFFS flash
# create SPIFF partition
spiffs_create_partition_image(storage spiffs FLASH_IN_PROJECT)

# Define the major version
set(MAJOR_VERSION "1.0")

# Find the path to the bash command
find_program(BASH_PROGRAM bash)

if(NOT BASH_PROGRAM)
    message(FATAL_ERROR "Bash not found. Please install Bash.")
endif()

message(STATUS "Bash found at: ${BASH_PROGRAM}")

# Add a custom target to run the make-version.sh script
add_custom_target(
    make_version ALL
    COMMAND ${BASH_PROGRAM} ${CMAKE_SOURCE_DIR}/util/make-version.sh ${CMAKE_SOURCE_DIR} ${MAJOR_VERSION}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running make-version.sh script"
)

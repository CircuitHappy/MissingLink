cmake_minimum_required(VERSION 3.0)

############
# Toolchain
############

set(CMAKE_FIND_ROOT_PATH  ${TARGET_DIR})

include_directories(/usr/include)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

##########
# Project
##########

project(missing_link)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Os -Wno-psabi")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(vendor/link/AbletonLinkConfig.cmake)

include_directories(src)
include_directories(${link_HEADERS})

file(
  GLOB_RECURSE
  missing_link_sources
  "src/missing_link/*.cpp"
)

add_executable(missing_link ${missing_link_sources})
target_link_libraries(missing_link atomic pthread config++ rtmidi Ableton::Link)

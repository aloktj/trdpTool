cmake_minimum_required(VERSION 3.16)
project(trdpTool LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(TRDP_INCLUDE_DIR "" CACHE PATH "Directory containing TRDP headers")
set(TRDP_LIB_PATH "" CACHE FILEPATH "Path to libtrdp.a or equivalent")

add_subdirectory(trdp-core)
add_subdirectory(apps/trdp-sim)


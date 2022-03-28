cmake_minimum_required(VERSION 3.16.3)
project(WebServer)
set(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_FLAGS -pthread)

include_directories(${PROJECT_SOURCE_DIR}/version_0.1/header)
aux_source_directory(${PROJECT_SOURCE_DIR}/version_0.1/source DIR_SRC)
add_executable(WebServer ${DIR_SRC})

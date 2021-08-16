cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)
set (CMAKE_CXX_STANDARD 20)

project(betterconfig)

include_directories(include)

add_executable( ${PROJECT_NAME}-test
  tests/main.cpp
)
target_link_libraries(${PROJECT_NAME}-test
	pthread
)

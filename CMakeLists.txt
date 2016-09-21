cmake_minimum_required(VERSION 2.8)
PROJECT(Akcp CXX C)
include_directories(3rdparty/asio-1.10.6/include 3rdparty)
link_directories(/home/liuyh/app/lib)

add_definitions(-std=c++11
				-DASIO_STANDALONE
				) 
#SET(CMAKE_BUILD_TYPE "Release")
SET(CMAKE_BUILD_TYPE "Debug")

add_subdirectory(src)
add_subdirectory(samples)

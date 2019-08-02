cmake_minimum_required(VERSION 3.6)
project(csx2dif)

add_subdirectory("3rdparty/DifBuilder")
add_subdirectory("3rdparty/tinyobjloader")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES main.cpp tinyxml2.cpp)
add_executable(csx2dif ${SOURCE_FILES})

include_directories(includes)
include_directories(3rdparty/tinyobjloader)
include_directories(3rdparty/DifBuilder/include)
include_directories(3rdparty/DifBuilder/3rdparty/Dif)
include_directories(3rdparty/DifBuilder/3rdparty/Dif/3rdparty/glm)
include_directories(3rdparty/DifBuilder/3rdparty/Dif/include)
target_link_libraries(csx2dif DifBuilder Dif tinyobjloader)
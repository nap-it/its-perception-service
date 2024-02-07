cmake_minimum_required(VERSION 3.10)
project(cpmGeneration)

set(CMAKE_CXX_STANDARD 17)  

include_directories(${CMAKE_SOURCE_DIR}/spdlog/include)
include_directories(${CMAKE_SOURCE_DIR}/rapidjson)
include_directories(${CMAKE_SOURCE_DIR}/fastdds)
include_directories(${CMAKE_SOURCE_DIR})

find_package(Threads REQUIRED)
find_package(Fastcdr REQUIRED)
find_package(Fastrtps REQUIRED)

file(GLOB FASTDDS_SRC
    ${CMAKE_SOURCE_DIR}/fastdds/*.cpp
    ${CMAKE_SOURCE_DIR}/fastdds/*.c
    ${CMAKE_SOURCE_DIR}/fastdds/*.h
    ${CMAKE_SOURCE_DIR}/fastdds/*.hpp
    ${CMAKE_SOURCE_DIR}/fastdds/*.cxx
)
add_executable(server server.cpp config_reader.hpp sensor_info.hpp sensor_info.cpp ${FASTDDS_SRC})

add_executable(adapter1 adapter1.cpp config_reader.hpp sensor_info.hpp sensor_info.cpp ${FASTDDS_SRC})

add_executable(adapter2 adapter2.cpp config_reader.hpp sensor_info.hpp sensor_info.cpp ${FASTDDS_SRC})

target_link_libraries(server pthread ssl crypto fastcdr fastrtps) 

target_link_libraries(adapter1 pthread ssl crypto fastcdr fastrtps)

target_link_libraries(adapter2 pthread ssl crypto fastcdr fastrtps)

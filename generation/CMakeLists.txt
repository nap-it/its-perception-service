cmake_minimum_required(VERSION 3.10)
project(cpmGeneration)

set(CMAKE_CXX_STANDARD 17)  

include_directories(${CMAKE_SOURCE_DIR}/spdlog/include)
include_directories(${CMAKE_SOURCE_DIR}/rapidjson)
include_directories(${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper)
include_directories(${CMAKE_SOURCE_DIR})

find_package(Threads REQUIRED)
find_package(Fastcdr REQUIRED)
find_package(Fastrtps REQUIRED)

file(GLOB FASTDDS_SRC
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.cpp
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.c
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.h
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.hpp
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.cxx
)

add_executable(server 
    server.cpp 
    config_reader.hpp 
    sensor_info.hpp 
    sensor_info.cpp 
    cpm_builder.hpp 
    cpm_builder.cpp 
    mqttwrapper.cpp 
    mqttwrapper.h 
    ${FASTDDS_SRC})

target_link_libraries(server pthread ssl crypto fastcdr fastrtps paho-mqttpp3 paho-mqtt3as) 


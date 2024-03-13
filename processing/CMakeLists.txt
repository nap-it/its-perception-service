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
add_executable(main 
    main.cpp 
    config_reader.hpp 
    cpm-processing.cpp 
    cpm-processing.h
    mqttwrapper.cpp 
    mqttwrapper.h 
    ${FASTDDS_SRC})

target_link_libraries(main pthread ssl crypto fastcdr fastrtps paho-mqttpp3 paho-mqtt3as) 
cmake_minimum_required(VERSION 3.10)
project(Generation)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/spdlog/include)
include_directories(${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper)
include_directories(${CMAKE_SOURCE_DIR}/rapidjson)
include_directories(${CMAKE_SOURCE_DIR}/paho-mqtt-cpp-wrapper/include)
include_directories(${CMAKE_SOURCE_DIR})

find_package(Threads REQUIRED)
find_package(Fastcdr REQUIRED)
find_package(Fastrtps REQUIRED)
find_package(zenohc REQUIRED)
find_package(zenohcxx REQUIRED)

add_subdirectory(paho-mqtt-cpp-wrapper)

file(GLOB FASTDDS_SRC
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.cpp
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.c
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.h
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.hpp
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.cxx
)

add_executable(main main.cpp ${FASTDDS_SRC}
        builder.cpp
        builder.h
        aggregator.cpp
        aggregator.h
        locator.cpp
        locator.h
        generation.cpp
        generation.h
)

target_link_libraries(main 
        PRIVATE
        pthread 
        fastcdr 
        fastrtps
        paho-mqtt-cpp-wrapper
        paho-mqttpp3 
        paho-mqtt3as
        zenohcxx::zenohc
)
cmake_minimum_required(VERSION 3.10)
project(RadarAdapterZenoh)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/spdlog/include)
include_directories(${CMAKE_SOURCE_DIR}/rapidjson)
include_directories(${CMAKE_SOURCE_DIR}/paho-mqtt-cpp-wrapper/include)
include_directories(${CMAKE_SOURCE_DIR})

find_package(Threads REQUIRED)
find_package(zenohc REQUIRED)
find_package(zenohcxx REQUIRED)

add_subdirectory(paho-mqtt-cpp-wrapper)

add_executable(main main.cpp 
        config_reader.h
        radar_adapter.cpp
        radar_adapter.h
)

target_link_libraries(main 
        pthread 
        zenohcxx::zenohc
        paho-mqtt-cpp-wrapper
        paho-mqttpp3 
        paho-mqtt3as
)
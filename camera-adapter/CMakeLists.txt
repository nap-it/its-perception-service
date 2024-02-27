cmake_minimum_required(VERSION 3.18.4)
project(camera_adapter)

set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/vendor)

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
add_executable(main main.cpp ${FASTDDS_SRC}
        mqtt.cpp
        mqtt.h
        config_reader.h
        camera_data_management.cpp
        camera_data_management.h
)

target_link_libraries(main pthread paho-mqttpp3 paho-mqtt3as ssl crypto fastcdr fastrtps)

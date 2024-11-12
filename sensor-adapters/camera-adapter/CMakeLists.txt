cmake_minimum_required(VERSION 3.18.4)
project(camera_adapter)

set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/spdlog/include)
include_directories(${CMAKE_SOURCE_DIR}/rapidjson)
include_directories(${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper)

find_package(Threads REQUIRED)
find_package(Fastcdr REQUIRED)
find_package(Fastrtps REQUIRED)

file(GLOB fastdds-cpp-wrapper_SRC
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.cpp
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.c
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.h
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.hpp
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.cxx
)
add_executable(main main.cpp ${fastdds-cpp-wrapper_SRC}
        mqttwrapper.cpp
        mqttwrapper.h
        config_reader.h
        camera_data_management.cpp
        camera_data_management.h
)

target_link_libraries(main pthread paho-mqttpp3 paho-mqtt3as ssl crypto fastcdr fastrtps)

cmake_minimum_required(VERSION 3.10)
project(RadarAdapter)

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

# find_path(PAHO_MQTT_C_INCLUDE_DIR NAMES MQTTAsync.h)
# find_library(PAHO_MQTT_C_LIB NAMES paho-mqtt3as paho-mqtt3c)
# find_path(PAHO_MQTT_CPP_INCLUDE_DIR NAMES mqtt/async_client.h)
# find_library(PAHO_MQTT_CPP_LIB NAMES paho-mqttpp3)
add_subdirectory(paho-mqtt-cpp-wrapper)

# include_directories(${PAHO_MQTT_C_INCLUDE_DIR} ${PAHO_MQTT_CPP_INCLUDE_DIR})

file(GLOB FASTDDS_SRC
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.cpp
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.c
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.h
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.hpp
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.cxx
)

add_executable(main main.cpp ${FASTDDS_SRC}
        #mqttwrapper.cpp
        #mqttwrapper.h
        builder.cpp
        builder.h
        aggregator.cpp
        aggregator.h
        locator.cpp
        locator.h
        generation.cpp
        generation.h
        nlohmann/json.hpp
)

target_link_libraries(main 
        pthread 
        fastcdr 
        fastrtps
        # ${PAHO_MQTT_C_LIB}
        # ${PAHO_MQTT_CPP_LIB}
        paho-mqtt-cpp-wrapper
        paho-mqttpp3 
        paho-mqtt3as
)
cmake_minimum_required(VERSION 3.10)
project(cpm_adapter)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -L/usr/local/lib")
# set(CMAKE_CXX_STANDARD 17)  

include_directories(${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper)
include_directories(${CMAKE_SOURCE_DIR}/rapidjson)
include_directories(${CMAKE_SOURCE_DIR}/spdlog/include)
include_directories(${CMAKE_SOURCE_DIR})

file(GLOB FASTDDS_SRC
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.cpp
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.c
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.h
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.hpp
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.cxx
)
find_package(Fastrtps REQUIRED)
find_package(Fastcdr REQUIRED) 

add_executable(main main.cpp ${FASTDDS_SRC} autoware_data_management.h autoware_data_management.cpp)

target_link_libraries(main fastrtps fastcdr pthread ${GeographicLib_LIBRARIES})

target_compile_features(main PUBLIC cxx_std_20)

install(TARGETS
  main
  DESTINATION lib/${PROJECT_NAME})

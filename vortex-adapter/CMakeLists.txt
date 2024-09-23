cmake_minimum_required(VERSION 3.10)
project(vortex_adapter)

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


# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(Fastrtps REQUIRED)
find_package(Fastcdr REQUIRED) 
find_package(tf2 REQUIRED)

find_package(vortex_perception_msgs REQUIRED)# HINTS "/home/amaral/Desktop/autoware_auto_msgs/install")


include_directories(
  ${vortex_perception_msgs_INCLUDE_DIRS},
)

# add_executable(talker src/publisher_member_function.cpp)
# ament_target_dependencies(talker rclcpp std_msgs)
# target_compile_features(talker PUBLIC cxx_std_20)

# add_executable(listener src/subscriber_member_function.cpp)
# ament_target_dependencies(listener rclcpp std_msgs geometry_msgs sensor_msgs vortex_perception_msgs tf2)
# target_link_libraries(listener fastrtps fastcdr)
# target_compile_features(listener PUBLIC cxx_std_20)

add_executable(main main.cpp ${FASTDDS_SRC})

ament_target_dependencies(main rclcpp std_msgs tf2 geometry_msgs vortex_perception_msgs Fastrtps Fastcdr)

target_link_libraries(main fastrtps fastcdr pthread )

target_compile_features(main PUBLIC cxx_std_20)

install(TARGETS
  main
  DESTINATION lib/${PROJECT_NAME})

ament_package()

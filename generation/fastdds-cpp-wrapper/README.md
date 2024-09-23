# fastdds-cpp-wrapper

## CMake

In your CMakeLists.txt file, add:
```
include_directories(${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper)

find_package(Fastcdr REQUIRED)
find_package(Fastrtps REQUIRED)

file(GLOB FASTDDS_SRC
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.cpp
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.c
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.h
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.hpp
        ${CMAKE_SOURCE_DIR}/fastdds-cpp-wrapper/*.cxx
)
```

And add 
```
${FASTDDS_SRC}
``` 
to your add_executable() instruction


## Usage

Full write-up pending
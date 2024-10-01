# Use OSRF ROS Humble as the base image
FROM ros:humble-ros-base-jammy

# Update the package list
RUN apt-get update 

# Install additional dependencies
RUN apt-get install -y \
    build-essential \
    cmake \
    git \
    libgeographic-dev \
    geographiclib-tools \
    libboost-date-time-dev \
    libboost-program-options-dev \
    libboost-system-dev \
    libcrypto++-dev \
    libasio-dev \
    libtinyxml2-dev \
    libcurl4-openssl-dev \
    libssl-dev \
    git \
    ca-certificates \
    ros-humble-rmw-cyclonedds-cpp \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /
RUN mkdir /vortex_adapter
COPY *.cpp /vortex_adapter/
COPY *.h /vortex_adapter/
COPY *.hpp /vortex_adapter/
COPY CMakeLists.txt /vortex_adapter/
COPY package.xml /vortex_adapter/

COPY fastdds-cpp-wrapper /vortex_adapter/fastdds-cpp-wrapper
COPY rapidjson /vortex_adapter/rapidjson
COPY spdlog /vortex_adapter/spdlog

WORKDIR /vortex_adapter
COPY vortex_std_msgs /vortex_adapter/vortex_std_msgs
WORKDIR /vortex_adapter/vortex_std_msgs
RUN /bin/bash -c 'source /opt/ros/humble/setup.bash && colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-w"'

WORKDIR /vortex_adapter
COPY vortex_perception_msgs /vortex_adapter/vortex_perception_msgs
WORKDIR /vortex_adapter/vortex_perception_msgs
RUN /bin/bash -c 'source /opt/ros/humble/setup.bash && source /vortex_adapter/vortex_std_msgs/install/setup.bash && colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-w"'

RUN mkdir -p /vortex_adapter/build

RUN /bin/bash -c "source /opt/ros/humble/setup.bash && source /vortex_adapter/vortex_perception_msgs/install/setup.bash && cd /vortex_adapter/build && cmake .. && make -j$(nproc)"

RUN chmod +x /vortex_adapter/build/main

# COPY start.sh /vortex_adapter/start.sh
# RUN chmod +x /vortex_adapter/start.sh

# ENTRYPOINT [ "/vortex_adapter/start.sh" ]

FROM debian:bullseye-slim
ENV TZ=Europe/Lisbon
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
RUN printf "deb http://httpredir.debian.org/debian bullseye-backports main non-free\ndeb-src http://httpredir.debian.org/debian bullseye-backports main non-free\n" > /etc/apt/sources.list.d/backports.list
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake/bullseye-backports \
    cmake-data/bullseye-backports \
    libboost-date-time-dev \
    libboost-program-options-dev \
    libboost-system-dev \
    libcrypto++-dev \
    libasio-dev \
    libtinyxml2-dev \
    libcurl4-openssl-dev \
    libssl-dev \
    git \
    ca-certificates

# fastdds
WORKDIR /tmp
RUN git clone https://github.com/eProsima/foonathan_memory_vendor.git
RUN mkdir /tmp/foonathan_memory_vendor/build
WORKDIR /tmp/foonathan_memory_vendor/build
RUN cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local/ -DBUILD_SHARED_LIBS=ON
RUN cmake --build . --target install --parallel $(nproc)
RUN ldconfig
WORKDIR /tmp
RUN git clone https://github.com/eProsima/Fast-CDR.git
RUN mkdir /tmp/Fast-CDR/build
WORKDIR /tmp/Fast-CDR/build
RUN cmake ..
RUN cmake --build . --target install --parallel $(nproc)
WORKDIR /tmp
RUN git clone https://github.com/eProsima/Fast-DDS.git
WORKDIR /tmp/Fast-DDS/
RUN git checkout v2.13.2
RUN mkdir /tmp/Fast-DDS/build
WORKDIR /tmp/Fast-DDS/build
RUN cmake ..
RUN cmake --build . --target install --parallel $(nproc)

WORKDIR /
RUN mkdir /cpm_adapter
COPY *.cpp /cpm_adapter/
COPY *.h /cpm_adapter/
COPY *.hpp /cpm_adapter/
COPY CMakeLists.txt /cpm_adapter/
COPY package.xml /cpm_adapter/

WORKDIR /cpm_adapter
RUN git clone https://code.nap.av.it.pt/mobility-networks/fastdds-cpp-wrapper.git
# COPY fastdds-cpp-wrapper /cpm_adapter/fastdds-cpp-wrapper
COPY rapidjson /cpm_adapter/rapidjson
COPY spdlog /cpm_adapter/spdlog

# WORKDIR /cpm_adapter
# RUN git clone https://github.com/tier4/autoware_auto_msgs.git
# WORKDIR /cpm_adapter/autoware_auto_msgs
# RUN /bin/bash -c 'source /opt/ros/humble/setup.bash && colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-w"'

RUN mkdir -p /cpm_adapter/times

RUN mkdir -p /cpm_adapter/build

RUN /bin/bash -c "cd /cpm_adapter/build && cmake .. && make -j$(nproc)"

RUN chmod +x /cpm_adapter/build/main

COPY start.sh /cpm_adapter/start.sh
RUN chmod +x /cpm_adapter/start.sh

ENTRYPOINT [ "/cpm_adapter/start.sh" ]

FROM debian:bullseye-slim
ENV TZ=Europe/Lisbon
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    libboost-date-time-dev \
    libboost-program-options-dev \
    libboost-system-dev \
    libcrypto++-dev \
    libfastcdr-dev \
    libfastrtps-dev \
    git \
    ca-certificates
WORKDIR /

RUN mkdir /cpm-generation

COPY *.cpp /cpm-generation/
COPY *.h /cpm-generation/
COPY *.hpp /cpm-generation/
COPY CMakeLists.txt /cpm-generation/
COPY rapidjson /cpm-generation/rapidjson
COPY spdlog /cpm-generation/spdlog
COPY fastdds /cpm-generation/fastdds

RUN git clone https://github.com/eclipse/paho.mqtt.c.git
WORKDIR paho.mqtt.c
RUN git checkout v1.3.13
RUN cmake -Bbuild -H. -DPAHO_ENABLE_TESTING=OFF -DPAHO_BUILD_STATIC=ON \
        -DPAHO_WITH_SSL=ON -DPAHO_HIGH_PERFORMANCE=ON
RUN cmake --build build/ --target install
RUN ldconfig

WORKDIR /
RUN git clone https://github.com/eclipse/paho.mqtt.cpp
WORKDIR /paho.mqtt.cpp
RUN cmake -Bbuild -H. -DPAHO_BUILD_STATIC=ON \
        -DPAHO_BUILD_DOCUMENTATION=OFF -DPAHO_BUILD_SAMPLES=ON
RUN cmake --build build/ --target install
RUN ldconfig

#RUN git clone https://code.nap.av.it.pt/external-tools/rapidjson.git
#RUN git clone https://code.nap.av.it.pt/external-tools/spdlog.git

WORKDIR /cpm-generation
RUN cmake .
RUN cmake --build .
RUN chmod +x server
ENTRYPOINT ["/cpm-generation/server"]


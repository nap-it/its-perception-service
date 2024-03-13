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

RUN mkdir /radar-adapter

COPY *.cpp /radar-adapter/
COPY *.h /radar-adapter/
COPY *.hpp /radar-adapter/
COPY CMakeLists.txt /radar-adapter/
COPY rapidjson /radar-adapter/rapidjson
COPY spdlog /radar-adapter/spdlog
COPY fastdds /radar-adapter/fastdds

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

# WORKDIR /radar-adapter
# RUN git -c http.sslVerify=false clone https://code.nap.av.it.pt/external-tools/rapidjson.git
# RUN git -c http.sslVerify=false clone https://code.nap.av.it.pt/external-tools/spdlog.git

WORKDIR /radar-adapter
RUN cmake .
RUN cmake --build .
RUN chmod +x main
ENTRYPOINT ["/radar-adapter/main"]

#ENTRYPOINT ["sleep"]
#CMD 50000
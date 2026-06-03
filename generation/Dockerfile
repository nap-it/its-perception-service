# ==============================
# BUILDER STAGE
# ==============================
FROM debian:bullseye-slim AS builder

ARG CARGO_HOME=/root/.cargo

ENV TZ=Europe/Lisbon
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN printf "deb http://archive.debian.org/debian-archive/debian bullseye-backports main non-free\ndeb-src http://archive.debian.org/debian-archive/debian bullseye-backports main non-free\n" > /etc/apt/sources.list.d/backports.list

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
    clang \
    git \
    ca-certificates \
    curl \
    pkg-config \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# ------------------------- Zenoh Libraries -------------------------
RUN curl https://sh.rustup.rs -sSf | bash -s -- -y
ENV PATH="${CARGO_HOME}/bin:${PATH}"
RUN cargo --version

WORKDIR /tmp
RUN git clone https://github.com/eclipse-zenoh/zenoh-c.git \
    && cd zenoh-c \
    && git checkout 1.4.0 \
    && mkdir build && cd build \
    && cmake .. -DZENOHC_BUILD_WITH_UNSTABLE_API=true -DZENOHC_BUILD_WITH_SHARED_MEMORY=true -DCMAKE_INSTALL_PREFIX=/usr/local \
    && cmake --build . --config Release \
    && cmake --build . --target install

WORKDIR /tmp
RUN git clone https://github.com/eclipse-zenoh/zenoh-cpp.git \
    && cd zenoh-cpp \
    && git checkout 1.4.0 \
    && mkdir build && cd build \
    && cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DZENOHCXX_ENABLE_TESTS=OFF -DZENOHCXX_ENABLE_EXAMPLES=OFF -Dzenohc_DIR=/usr/local/lib/cmake/zenohc \
    && cmake --build . --target install

# ------------------------- DDS Libraries -------------------------
WORKDIR /tmp
RUN git clone https://github.com/eProsima/foonathan_memory_vendor.git
RUN mkdir /tmp/foonathan_memory_vendor/build
WORKDIR /tmp/foonathan_memory_vendor/build
RUN cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local/ -DBUILD_SHARED_LIBS=ON
RUN cmake --build . --target install --parallel $(nproc)
WORKDIR /tmp
RUN git clone https://github.com/eProsima/Fast-CDR.git
WORKDIR /tmp/Fast-CDR/
RUN git checkout v2.2.6
RUN mkdir /tmp/Fast-CDR/build
WORKDIR /tmp/Fast-CDR/build
RUN cmake ..
RUN cmake --build . --target install --parallel $(nproc)
WORKDIR /tmp
RUN git clone https://github.com/eProsima/Fast-DDS.git
WORKDIR /tmp/Fast-DDS/
RUN git checkout v2.14.6
RUN mkdir /tmp/Fast-DDS/build
WORKDIR /tmp/Fast-DDS/build
RUN cmake ..
RUN cmake --build . --target install --parallel $(nproc)

# ------------------------- MQTT Libraries -------------------------
WORKDIR /tmp
RUN git clone https://github.com/eclipse/paho.mqtt.c.git
WORKDIR /tmp/paho.mqtt.c
RUN git checkout v1.3.13
RUN cmake -Bbuild -H. -DPAHO_ENABLE_TESTING=OFF -DPAHO_BUILD_STATIC=ON -DPAHO_WITH_SSL=ON -DPAHO_HIGH_PERFORMANCE=ON
RUN cmake --build build/ --target install
RUN ldconfig

WORKDIR /tmp
RUN git clone https://github.com/eclipse/paho.mqtt.cpp
WORKDIR /tmp/paho.mqtt.cpp
RUN git checkout v1.3.2
RUN cmake -Bbuild -H. -DPAHO_BUILD_STATIC=ON -DPAHO_BUILD_DOCUMENTATION=OFF -DPAHO_BUILD_SAMPLES=ON
RUN cmake --build build/ --target install
RUN ldconfig

# ------------------------- Install Prometheus C++ Client -------------------------
WORKDIR /usr/src
RUN git clone https://github.com/jupp0r/prometheus-cpp.git
WORKDIR /usr/src/prometheus-cpp
RUN git submodule update --init --recursive
RUN mkdir build && cd build
WORKDIR /usr/src/prometheus-cpp/build
RUN cmake .. -DBUILD_SHARED_LIBS=ON  -DENABLE_PULL=ON -DENABLE_CIVETWEB=ON -DENABLE_PUSH=OFF -DENABLE_TESTING=OFF
RUN cmake --build . --parallel $(nproc)
RUN cmake --build . --target install
RUN ldconfig

# ------------------------- Build Generation -------------------------
WORKDIR /usr/src/generation
RUN git -c http.sslVerify=false clone https://github.com/gabime/spdlog.git
WORKDIR /usr/src/generation/spdlog
RUN git checkout v1.11.0
WORKDIR /usr/src/generation
RUN git -c http.sslVerify=false clone https://github.com/nap-it/fastdds-cpp-wrapper.git
RUN git -c http.sslVerify=false clone https://github.com/nap-it/paho.mqtt.cpp.wrapper.git paho-mqtt-cpp-wrapper
RUN git -c http.sslVerify=false clone https://github.com/Tencent/rapidjson.git
WORKDIR /usr/src/generation/rapidjson
RUN git checkout 012be8528783cdbf4b7a9e64f78bd8f056b97e24 \
    && mv include/rapidjson/* . && rmdir include/rapidjson include

COPY *.cpp /usr/src/generation/
COPY *.h /usr/src/generation/
COPY *.hpp /usr/src/generation/
COPY CMakeLists.txt /usr/src/generation/
COPY zenoh_config.json5 /usr/src/generation/

WORKDIR /usr/src/generation
RUN cmake . && cmake --build . --parallel $(nproc)
RUN chmod +x main


# ==============================
# FINAL RUNTIME STAGE
# ==============================
FROM debian:bullseye-slim AS runtime

ENV TZ=Europe/Lisbon
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# Install only necessary runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    libboost-date-time1.74.0 \
    libboost-program-options1.74.0 \
    libboost-system1.74.0 \
    libcrypto++-dev \
    libasio-dev \
    libtinyxml2-8 \
    libcurl4-openssl-dev \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

# ------------------------- Copy Built Libraries -------------------------
COPY --from=builder /usr/local/lib/libpaho-mqtt3*.so* /usr/local/lib/
COPY --from=builder /usr/local/lib/libpaho-mqttpp3.so* /usr/local/lib/
COPY --from=builder /usr/local/lib/libfastcdr.so* /usr/local/lib/
COPY --from=builder /usr/local/lib/libfastrtps.so* /usr/local/lib/
COPY --from=builder /usr/local/lib/libfoonathan_memory* /usr/local/lib/
COPY --from=builder /usr/local/lib/libspdlog.so* /usr/local/lib/
COPY --from=builder /usr/local/lib/libzenoh* /usr/local/lib/
COPY --from=builder /usr/local/lib/libprometheus-cpp*.so* /usr/local/lib/

RUN ldconfig

# ------------------------- Copy Generation Binary -------------------------
COPY --from=builder /usr/src/generation/main /generation/main
COPY --from=builder /usr/src/generation/zenoh_config.json5 /generation/zenoh_config.json5

WORKDIR /generation
ENTRYPOINT ["/generation/main"]
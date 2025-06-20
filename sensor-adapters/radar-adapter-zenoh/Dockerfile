# ==============================
# BUILDER STAGE
# ==============================
FROM debian:bullseye-slim AS builder

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
    ca-certificates \
    clang \
    pkg-config \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Install Rust toolchain (needed to build zenoh-c)
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | bash -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"

# ------------------------- Zenoh C -------------------------
WORKDIR /workspace
RUN git clone https://github.com/eclipse-zenoh/zenoh-c.git
WORKDIR /workspace/zenoh-c
RUN git checkout 1.4.0
WORKDIR /workspace/zenoh-c/build
RUN cmake .. -DZENOHC_BUILD_WITH_UNSTABLE_API=true -DZENOHC_BUILD_WITH_SHARED_MEMORY=true -DCMAKE_INSTALL_PREFIX=/usr/local && \
    cmake --build . --config Release && \
    cmake --build . --target install

# ------------------------- Zenoh CPP -------------------------
WORKDIR /workspace
RUN git clone https://github.com/eclipse-zenoh/zenoh-cpp.git
WORKDIR /workspace/zenoh-cpp
RUN git checkout 1.4.0
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DZENOHCXX_ENABLE_TESTS=OFF -DZENOHCXX_ENABLE_EXAMPLES=OFF -Dzenohc_DIR=/usr/local/lib/cmake/zenohc && \
    cmake --build . --target install

# ------------------------- Build MQTT C -------------------------
WORKDIR /usr/src
RUN git clone https://github.com/eclipse/paho.mqtt.c.git
WORKDIR /usr/src/paho.mqtt.c
RUN git checkout v1.3.13
RUN cmake -Bbuild -H. -DPAHO_ENABLE_TESTING=OFF -DPAHO_BUILD_STATIC=ON -DPAHO_WITH_SSL=ON -DPAHO_HIGH_PERFORMANCE=ON
RUN cmake --build build/ --target install
RUN ldconfig

# ------------------------- Build MQTT C++ -------------------------
WORKDIR /usr/src
RUN git clone https://github.com/eclipse/paho.mqtt.cpp
WORKDIR /usr/src/paho.mqtt.cpp
RUN git checkout v1.3.2
RUN cmake -Bbuild -H. -DPAHO_BUILD_STATIC=ON -DPAHO_BUILD_DOCUMENTATION=OFF -DPAHO_BUILD_SAMPLES=ON
RUN cmake --build build/ --target install
RUN ldconfig

# ------------------------- Build Radar Adapter -------------------------
WORKDIR /usr/src/radar-adapter
# RUN git -c http.sslVerify=false clone https://code.nap.av.it.pt/external-tools/spdlog.git
# RUN git -c http.sslVerify=false clone https://code.nap.av.it.pt/mobility-networks/paho-mqtt-cpp-wrapper.git
# RUN git -c http.sslVerify=false clone https://code.nap.av.it.pt/external-tools/rapidjson.git
COPY spdlog /usr/src/radar-adapter/spdlog
COPY paho-mqtt-cpp-wrapper /usr/src/radar-adapter/paho-mqtt-cpp-wrapper
COPY rapidjson /usr/src/radar-adapter/rapidjson

COPY *.cpp /usr/src/radar-adapter/
COPY *.h /usr/src/radar-adapter/
COPY *.hpp /usr/src/radar-adapter/
COPY CMakeLists.txt /usr/src/radar-adapter/
COPY zenoh_config.json5 /usr/src/radar-adapter/

WORKDIR /usr/src/radar-adapter
RUN cmake . && cmake --build . --parallel $(nproc)
RUN chmod +x main

# ==============================
# FINAL RUNTIME STAGE
# ==============================
FROM debian:bullseye-slim AS runtime

ENV TZ=Europe/Lisbon
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN printf "deb http://httpredir.debian.org/debian bullseye-backports main non-free\ndeb-src http://httpredir.debian.org/debian bullseye-backports main non-free\n" > /etc/apt/sources.list.d/backports.list

# Install only necessary runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake/bullseye-backports \
    cmake-data/bullseye-backports \
    libboost-date-time1.74.0 \
    libboost-program-options1.74.0 \
    libboost-system1.74.0 \
    libcrypto++-dev \
    libasio-dev \
    libtinyxml2-8 \
    libcurl4-openssl-dev \
    libssl-dev \
    git \
    clang \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# ------------------------- Copy Built Libraries -------------------------
COPY --from=builder /usr/local/lib/libzenoh* /usr/local/lib/
COPY --from=builder /usr/local/lib/libpaho-mqtt3*.so* /usr/local/lib/
COPY --from=builder /usr/local/lib/libpaho-mqttpp3.so* /usr/local/lib/
COPY --from=builder /usr/local/lib/libspdlog.so* /usr/local/lib/
# COPY --from=builder /usr/local/ /usr/local/

# Ensure shared libraries are found
RUN ldconfig

# ------------------------- Copy Radar Adapter Binary -------------------------
COPY --from=builder /usr/src/radar-adapter/main /radar-adapter/main
COPY --from=builder /usr/src/radar-adapter/zenoh_config.json5 /radar-adapter/zenoh_config.json5
# RUN mkdir -p /radar-adapter
# RUN cp /usr/src/radar-adapter/main /radar-adapter/main

WORKDIR /radar-adapter
ENTRYPOINT ["/radar-adapter/main"]
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
    clang \
    git \
    ca-certificates \
    curl \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Install Rust toolchain (needed to build zenoh-c)
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | bash -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"

# ------------------------- Zenoh C -------------------------
WORKDIR /workspace
RUN git clone https://github.com/eclipse-zenoh/zenoh-c.git
WORKDIR /workspace/zenoh-c
RUN git checkout 1.4.0
RUN mkdir build && cd build && \
    cmake .. -DZENOHC_BUILD_WITH_UNSTABLE_API=true -DZENOHC_BUILD_WITH_SHARED_MEMORY=true -DCMAKE_INSTALL_PREFIX=/usr/local && \
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

# ------------------------- Fast-DDS -------------------------
WORKDIR /usr/src
RUN git clone https://github.com/eProsima/foonathan_memory_vendor.git && \
    mkdir /usr/src/foonathan_memory_vendor/build
WORKDIR /usr/src/foonathan_memory_vendor
RUN git checkout v1.3.1
WORKDIR /usr/src/foonathan_memory_vendor/build
RUN cmake .. -DBUILD_SHARED_LIBS=ON && \
    cmake --build . --target install --parallel $(nproc)

WORKDIR /usr/src
RUN git clone https://github.com/eProsima/Fast-CDR.git && \
    mkdir /usr/src/Fast-CDR/build
WORKDIR /usr/src/Fast-CDR/
RUN git checkout v2.2.6
WORKDIR /usr/src/Fast-CDR/build
RUN cmake .. && \
    cmake --build . --target install --parallel $(nproc)

WORKDIR /usr/src
RUN git clone https://github.com/eProsima/Fast-DDS.git
WORKDIR /usr/src/Fast-DDS/
RUN git checkout v2.14.4
RUN mkdir /usr/src/Fast-DDS/build
WORKDIR /usr/src/Fast-DDS/build
RUN cmake .. && \
    cmake --build . --target install --parallel $(nproc)

# ------------------------- MQTT C -------------------------
WORKDIR /usr/src
RUN git clone https://github.com/eclipse/paho.mqtt.c.git
WORKDIR /usr/src/paho.mqtt.c
RUN git checkout v1.3.13
RUN cmake -Bbuild -H. -DPAHO_ENABLE_TESTING=OFF -DPAHO_BUILD_STATIC=ON -DPAHO_WITH_SSL=ON -DPAHO_HIGH_PERFORMANCE=ON
RUN cmake --build build/ --target install
RUN ldconfig

# ------------------------- MQTT C++ -------------------------
WORKDIR /usr/src
RUN git clone https://github.com/eclipse/paho.mqtt.cpp
WORKDIR /usr/src/paho.mqtt.cpp
RUN git checkout v1.3.2
RUN cmake -Bbuild -H. -DPAHO_BUILD_STATIC=ON -DPAHO_BUILD_DOCUMENTATION=OFF -DPAHO_BUILD_SAMPLES=ON
RUN cmake --build build/ --target install
RUN ldconfig

# ------------------------- Build Generation -------------------------
WORKDIR /usr/src/generation
RUN git -c http.sslVerify=false clone https://code.nap.av.it.pt/external-tools/spdlog.git
RUN git -c http.sslVerify=false clone https://code.nap.av.it.pt/mobility-networks/fastdds-cpp-wrapper.git
RUN git -c http.sslVerify=false clone https://code.nap.av.it.pt/mobility-networks/paho-mqtt-cpp-wrapper.git
RUN git -c http.sslVerify=false clone https://code.nap.av.it.pt/external-tools/rapidjson.git

COPY *.cpp /usr/src/generation/
COPY *.h /usr/src/generation/
COPY *.hpp /usr/src/generation/
COPY CMakeLists.txt /usr/src/generation/

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

# Ensure shared libraries are found
RUN ldconfig

# ------------------------- Copy Generation Binary -------------------------
COPY --from=builder /usr/src/generation/main /generation/main
# RUN mkdir /generation/logs
WORKDIR /generation
ENTRYPOINT ["/generation/main"]
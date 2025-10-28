# ==============================
# BUILDER STAGE
# ==============================
FROM code.nap.av.it.pt:5050/mobility-networks/middleware-images/zenoh-libs:1.4.0 AS zenoh-libs
FROM code.nap.av.it.pt:5050/mobility-networks/middleware-images/dds-libs:2.14.4 AS dds-libs
FROM code.nap.av.it.pt:5050/mobility-networks/middleware-images/mqtt-libs:1.3.2 AS mqtt-libs

FROM code.nap.av.it.pt:5050/external-tools/misc-docker-images/debian:bullseye-slim AS builder

COPY --from=zenoh-libs /usr/local /usr/local
COPY --from=dds-libs /usr/local /usr/local
COPY --from=mqtt-libs /usr/local /usr/local

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
RUN git -c http.sslVerify=false clone https://code.nap.av.it.pt/external-tools/spdlog.git
RUN git -c http.sslVerify=false clone https://code.nap.av.it.pt/mobility-networks/fastdds-cpp-wrapper.git
RUN git -c http.sslVerify=false clone https://code.nap.av.it.pt/mobility-networks/paho-mqtt-cpp-wrapper.git
RUN git -c http.sslVerify=false clone https://code.nap.av.it.pt/external-tools/rapidjson.git

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
FROM code.nap.av.it.pt:5050/external-tools/misc-docker-images/debian:bullseye-slim AS runtime

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
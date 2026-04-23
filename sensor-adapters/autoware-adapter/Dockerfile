# ==============================
# BUILDER STAGE
# ==============================
FROM code.nap.av.it.pt:5050/mobility-networks/middleware-images/dds-libs:2.14.4 AS dds-libs

FROM code.nap.av.it.pt:5050/external-tools/misc-docker-images/debian:bullseye-slim AS builder

COPY --from=dds-libs /usr/local /usr/local

ENV TZ=Europe/Lisbon
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN printf "deb http://archive.debian.org/debian-archive/debian/ bullseye-backports main non-free\ndeb-src http://archive.debian.org/debian-archive/debian/ bullseye-backports main non-free\n" > /etc/apt/sources.list.d/backports.list

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
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# ------------------------- Build Autoware Adapter -------------------------
WORKDIR /usr/src/autoware-adapter
RUN git -c http.sslVerify=false clone https://code.nap.av.it.pt/external-tools/spdlog.git
RUN git -c http.sslVerify=false clone https://github.com/nap-it/fastdds-cpp-wrapper.git
RUN git -c http.sslVerify=false clone https://code.nap.av.it.pt/external-tools/rapidjson.git

COPY *.cpp /usr/src/autoware-adapter/
COPY *.h /usr/src/autoware-adapter/
COPY *.hpp /usr/src/autoware-adapter/
COPY CMakeLists.txt /usr/src/autoware-adapter/

WORKDIR /usr/src/autoware-adapter
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
    python3 \
    && rm -rf /var/lib/apt/lists/*

# ------------------------- Copy Built Libraries -------------------------
COPY --from=builder /usr/local/lib/libfastcdr.so* /usr/local/lib/
COPY --from=builder /usr/local/lib/libfastrtps.so* /usr/local/lib/
COPY --from=builder /usr/local/lib/libfoonathan_memory* /usr/local/lib/
COPY --from=builder /usr/local/lib/libspdlog.so* /usr/local/lib/

# Ensure shared libraries are found
RUN ldconfig

# ------------------------- Copy Autoware Adapter Binary -------------------------
COPY --from=builder /usr/src/autoware-adapter/main /autoware-adapter/main

WORKDIR /autoware-adapter
ENTRYPOINT ["/autoware-adapter/main"]
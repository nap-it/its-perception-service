# ==============================
# BUILDER STAGE
# ==============================

FROM debian:bullseye-slim AS builder

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
    curl \
    && rm -rf /var/lib/apt/lists/*

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

# ------------------------- Build Autoware Adapter -------------------------
WORKDIR /usr/src/autoware-adapter
RUN git -c http.sslVerify=false clone https://github.com/gabime/spdlog.git
WORKDIR /usr/src/autoware-adapter/spdlog
RUN git checkout v1.11.0
WORKDIR /usr/src/autoware-adapter
RUN git -c http.sslVerify=false clone https://github.com/nap-it/fastdds-cpp-wrapper.git
RUN git -c http.sslVerify=false clone https://github.com/Tencent/rapidjson.git
WORKDIR /usr/src/autoware-adapter/rapidjson
RUN git checkout 012be8528783cdbf4b7a9e64f78bd8f056b97e24 \
    && mv include/rapidjson/* . && rmdir include/rapidjson include

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
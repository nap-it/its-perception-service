FROM debian:bullseye-slim
ENV TZ=Europe/Lisbon
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
#RUN printf "deb http://httpredir.debian.org/debian buster-backports main non-free\ndeb-src http://httpredir.debian.org/debian buster-backports main non-free\n" > /etc/apt/sources.list.d/backports.list
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    libboost-date-time-dev \
    libboost-program-options-dev \
    libboost-system-dev \
    libcrypto++-dev \
    libfastcdr-dev \
    libfastrtps-dev
WORKDIR /
COPY . /
RUN cmake .
RUN cmake --build .
RUN chmod +x main
ENTRYPOINT ["/main"]
FROM ubuntu:latest

RUN apt-get update && apt-get install -y \
    clang \
    make \
    cmake \
    libvulkan-dev \
    libvulkan1 \
    libsdl2-dev \
    libssl-dev \
    libopenal-dev

ENV CC=clang
ENV CXX=clang++

WORKDIR /app

COPY ./src ./src
COPY ./include ./include
COPY ./res/shaders ./res/shaders
COPY ./res/scripts ./res/scripts
COPY ./res/models/gaussian_splatting/ ./res/models/gaussian_splatting
COPY ./samples ./samples
COPY ./libdatachannel ./libdatachannel
COPY ./zlib ./zlib
COPY ./build.sh ./build.sh
COPY ./CMakeLists.txt ./CMakeLists.txt
RUN set -e; \
    mkdir -p build; \
    cd build; \
    cmake ..; \
    make -j4; \
    cd ..;

# Command to run when the container starts
CMD ["./build/samples/hyperion-sample"]

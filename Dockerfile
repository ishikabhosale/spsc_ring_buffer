FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive \
    TZ=Asia/Tokyo

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    ca-certificates \
    libbenchmark-dev \
    libboost-dev \
    linux-tools-common \
    linux-tools-generic \
    tzdata \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

CMD ["bash"]

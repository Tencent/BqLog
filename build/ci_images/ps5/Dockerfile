FROM ubuntu:24.04

# ps5-payload-sdk release to install (see https://github.com/ps5-payload-dev/sdk/releases)
ARG PS5_SDK_VERSION=v0.41

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        clang-18 \
        lld-18 \
        llvm-18 \
        cmake \
        make \
        ninja-build \
        curl \
        unzip \
        ca-certificates \
        git \
        python3 \
    && rm -rf /var/lib/apt/lists/*

RUN curl -sSL -o /tmp/ps5-payload-sdk.zip \
        "https://github.com/ps5-payload-dev/sdk/releases/download/${PS5_SDK_VERSION}/ps5-payload-sdk.zip" \
    && unzip -q /tmp/ps5-payload-sdk.zip -d /opt \
    && rm -f /tmp/ps5-payload-sdk.zip

ENV PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk

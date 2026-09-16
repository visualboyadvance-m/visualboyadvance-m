FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    nasm \
    cmake \
    ninja-build \
    ccache \
    gettext \
    zlib1g-dev \
    libgl1-mesa-dev \
    libgettextpo-dev \
    libsdl2-dev \
    libglu1-mesa-dev \
    libgles2-mesa-dev \
    libglew-dev \
    libwxgtk3.2-dev \
    libgtk-3-dev \
    libopenal-dev \
    libasound2-dev \
    zip \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

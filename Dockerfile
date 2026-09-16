FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

# Bootstrap the minimum needed to run installdeps
RUN apt-get update && apt-get install -y --no-install-recommends \
    sudo \
    lsb-release \
    && rm -rf /var/lib/apt/lists/*

# Use the project's own installdeps script to install all build dependencies
COPY installdeps /tmp/installdeps
RUN /tmp/installdeps

# Additional dep for vendored RtMidi (ALSA MIDI sequencer)
RUN apt-get install -y --no-install-recommends libasound2-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

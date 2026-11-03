# Linux build image for the server, tests and a compile check of the client.
FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential cmake git ca-certificates \
      libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev \
      xvfb xdotool imagemagick libgl1-mesa-dri \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src

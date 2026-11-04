#!/usr/bin/env sh
# Cross-compile Windows exes into build-windows/ inside the Linux image. Run from the repo root.
set -e
docker image inspect dodge-build >/dev/null 2>&1 || docker build -q -t dodge-build . >/dev/null
src="$(pwd -W 2>/dev/null || pwd)"  # pwd -W gives a Windows path under Git Bash
MSYS_NO_PATHCONV=1 docker run --rm -v "$src:/src" dodge-build sh -c \
  "cmake -S . -B build-windows -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake && cmake --build build-windows -j"

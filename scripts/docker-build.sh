#!/usr/bin/env sh
# Build everything and run tests inside the Linux image. Run from the repo root.
set -e
docker build -q -t dodge-build . >/dev/null
src="$(pwd -W 2>/dev/null || pwd)"  # pwd -W gives a Windows path under Git Bash
MSYS_NO_PATHCONV=1 docker run --rm -v "$src:/src" dodge-build sh -c \
  "cmake -S . -B build-linux && cmake --build build-linux -j && ctest --test-dir build-linux --output-on-failure"

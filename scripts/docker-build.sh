#!/usr/bin/env sh
# Build everything and run tests inside the Linux image. Run from the repo root.
set -e
# Build the image only when missing, so offline rebuilds still work.
docker image inspect dodge-build >/dev/null 2>&1 || docker build -q -t dodge-build . >/dev/null
src="$(pwd -W 2>/dev/null || pwd)"  # pwd -W gives a Windows path under Git Bash
MSYS_NO_PATHCONV=1 docker run --rm -v "$src:/src" dodge-build sh -c \
  "cmake -S . -B build-linux && cmake --build build-linux -j && ctest --test-dir build-linux --output-on-failure"

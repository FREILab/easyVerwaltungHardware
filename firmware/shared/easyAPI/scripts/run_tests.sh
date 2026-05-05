#!/usr/bin/env bash

set -e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

echo "[easyAPI] Running tests..."

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake -DEASY_API_BUILD_TESTS=ON ..
cmake --build .

echo "[easyAPI] Executing tests..."
ctest --output-on-failure

echo "[easyAPI] Done."
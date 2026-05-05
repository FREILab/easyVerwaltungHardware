#!/usr/bin/env bash

set -e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

echo "[easyAPI] Running coverage..."

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake -DEASY_API_BUILD_TESTS=ON -DEASY_API_ENABLE_COVERAGE=ON ..
cmake --build .

ctest --output-on-failure

echo "[easyAPI] Generating coverage report..."

lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/tests/*' \
  --ignore-errors unused \
  --output-file coverage.filtered.info
genhtml coverage.filtered.info --output-directory coverage_html

echo "[easyAPI] Coverage report generated:"
echo "  $BUILD_DIR/coverage_html/index.html"
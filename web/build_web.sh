#!/usr/bin/env bash
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_DIR="${SCRIPT_DIR}/build"

echo "Building MREmu WebAssembly project with Emscripten..."

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

emcmake cmake .. -DCMAKE_BUILD_TYPE=Release
emmake make -j$(nproc 2>/dev/null || echo 4)

echo "Web build complete! Artifacts generated in ${BUILD_DIR}/"

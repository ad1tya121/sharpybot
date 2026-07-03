#!/usr/bin/env bash
set -euo pipefail

REPO_URL="https://github.com/ad1tya121/sharpybot.git"
REPO_DIR="sharpybot"

if [ -d "$REPO_DIR/.git" ]; then
    echo "==> Updating existing checkout..."
    git -C "$REPO_DIR" pull --ff-only
else
    echo "==> Cloning sharpybot..."
    git clone "$REPO_URL" "$REPO_DIR"
fi

cd "$REPO_DIR/engine"

BUILD_DIR="build"
JOBS="$( (command -v nproc >/dev/null && nproc) || (command -v sysctl >/dev/null && sysctl -n hw.ncpu) || echo 4 )"

echo "==> Configuring..."
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

echo "==> Building (using $JOBS jobs)..."
cmake --build "$BUILD_DIR" --config Release -j"$JOBS"

BIN=$(find "$BUILD_DIR" -type f -name "engine" | head -n 1)

if [ -z "${BIN}" ]; then
    echo "ERROR: could not find the built 'engine' binary under $BUILD_DIR" >&2
    exit 1
fi

cp -f model.bin "$(dirname "$BIN")/"

echo "==> Build complete."
echo "    Engine binary: $(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")"
echo "    (point Cute Chess to this file, with working directory set to the same folder)"

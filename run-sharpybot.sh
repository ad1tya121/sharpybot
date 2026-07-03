#!/usr/bin/env bash
# Download and build the sharpybot chess engine on Linux/macOS.
# Usage: ./run-sharpybot.sh
set -euo pipefail

REPO_URL="https://github.com/ad1tya121/sharpybot.git"
REPO_DIR="sharpybot"

# --- 1. Get the code -------------------------------------------------------
if [ -d "$REPO_DIR/.git" ]; then
    echo "==> Updating existing checkout..."
    git -C "$REPO_DIR" pull --ff-only
else
    echo "==> Cloning sharpybot..."
    git clone "$REPO_URL" "$REPO_DIR"
fi

cd "$REPO_DIR/engine"

# --- 2. Configure & build ---------------------------------------------------
BUILD_DIR="build"
JOBS="$( (command -v nproc >/dev/null && nproc) || (command -v sysctl >/dev/null && sysctl -n hw.ncpu) || echo 4 )"

echo "==> Configuring..."
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

echo "==> Building (using $JOBS jobs)..."
cmake --build "$BUILD_DIR" --config Release -j"$JOBS"

# --- 3. Locate the built binary --------------------------------------------
BIN="$BUILD_DIR/engine"
if [ ! -f "$BIN" ]; then
    BIN="$(find "$BUILD_DIR" -maxdepth 2 -type f -name 'engine' | head -1)"
fi
if [ -z "${BIN:-}" ] || [ ! -f "$BIN" ]; then
    echo "ERROR: could not find the built 'engine' binary under $BUILD_DIR" >&2
    exit 1
fi

# --- 4. model.bin must sit next to the executable (loaded via relative path)
cp -f model.bin "$(dirname "$BIN")/"

echo "==> Build complete."
echo "    Engine binary: $(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")"
echo "    (point Cute Chess to this file, with working directory set to the same folder)"

#!/usr/bin/env bash
# Clean-builds RitOS and produces build/ritos.iso
set -e

cd "$(dirname "${BASH_SOURCE[0]}")/.."
JOBS=$(nproc 2>/dev/null || echo 4)

echo "[RitOS Build] Cleaning old files..."
make clean

echo "[RitOS Build] Compiling RitOS and generating ISO (-j$JOBS)..."
make -j"$JOBS"

echo "[RitOS Build] Success! Generated build/ritos.iso"

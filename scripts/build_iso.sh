#!/usr/bin/env bash
set -e

# Get project root directory
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Set PATH to include local mtools for grub-mkrescue
export PATH="${PROJECT_ROOT}/local_mtools/usr/bin:${PATH}"

echo "[RitOS Build] Cleaning old files..."
make clean

echo "[RitOS Build] Compiling RitOS and generating ISO..."
make

echo "[RitOS Build] Success! Generated ritos.iso in the root directory."

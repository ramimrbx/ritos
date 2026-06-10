#!/usr/bin/env bash
# Clean-builds RitOS and produces build/ritos.iso
set -euo pipefail
cd "$(dirname "$0")"

JOBS=$(nproc 2>/dev/null || echo 4)

echo "==> Cleaning previous build"
make clean

echo "==> Building RitOS (-j$JOBS)"
make -j"$JOBS"

echo ""
echo "==> Done: $(ls -lh build/ritos.iso | awk '{print $9, "("$5")"}')"
echo "    Run it with: make run   (or: qemu-system-i386 -cdrom build/ritos.iso)"

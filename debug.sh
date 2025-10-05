#!/bin/bash

echo "Starting x86_64 GDB for kernel debugging on Apple Silicon..."
echo "Make sure QEMU is running with: ./run.sh gdb"
echo ""

# Use the correct x86_64 GDB from Docker with proper host mapping
docker run --platform linux/amd64 -it --rm \
  -v ${PWD}:/root \
  -w /root \
  --add-host=host.docker.internal:host-gateway \
  agodio/itba-so-multi-platform:3.0 \
  x86_64-linux-gnu-gdb
#!/bin/bash
# Set audio device variable !!

if [ "$(uname)" == "Darwin" ]; then
    audio="coreaudio"
else
    audio="pa"
fi

if [[ "$1" = "gdb" ]]; then
    echo "Starting in debug mode..."
    echo "Connect with: gdb -> target remote localhost:1234"
    sudo qemu-system-x86_64 -s -S -hda Image/x64BareBonesImage.qcow2 -m 512
else
    sudo qemu-system-x86_64 -hda Image/x64BareBonesImage.qcow2 -m 512
fi 

#!/bin/bash

OVMF=/usr/share/ovmf/ovmf_aarch64.bin

qemu-system-aarch64 -m 512 -cpu cortex-a57 -M virt \
 -bios $OVMF \
 -hda fat:rw:. \
 -nographic

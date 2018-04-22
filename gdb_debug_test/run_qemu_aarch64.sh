#!/bin/bash
OVMF=/usr/share/ovmf/ovmf_aarch64.bin

qemu-system-aarch64 -m 1024 -cpu cortex-a57 -M virt \
 -bios $OVMF \
 -hda fat:rw:. \
 -nographic \
 -net nic -net tap,ifname=tap0,script=./upscript.sh,downscript=./downscript.sh \
 -gdb tcp::3333

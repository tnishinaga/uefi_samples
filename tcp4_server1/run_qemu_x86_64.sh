#!/bin/sh -x

OVMF_CODE=/usr/share/ovmf/x64/OVMF_CODE.fd
OVMF_VARS=/usr/share/ovmf/x64/OVMF_VARS.fd

qemu-system-x86_64 -cpu qemu64 -m 512 \
  -drive if=pflash,format=raw,unit=0,file=$OVMF_CODE,readonly=on \
  -drive if=pflash,format=raw,unit=1,file=$OVMF_VARS,readonly=on \
  -hda fat:rw:. -nographic \
  -net nic -net tap,ifname=tap0,script=./upscript.sh,downscript=./downscript.sh

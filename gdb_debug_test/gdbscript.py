#!/usr/bin/env python
import time
import socket


# retrieve base address
c = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
c.connect(("192.168.20.20", 10000))
img_base = c.recv(32)
c.close()

img_base = img_base.decode("ascii")
img_base_int = int(img_base, 16)

# connect to gdb
gdb.execute("target remote :3333")
gdb.execute("add-symbol-file main.so 0x{addr:x}".format(addr=img_base_int))

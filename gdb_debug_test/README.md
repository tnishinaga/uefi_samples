# How to debug UEFI application

## Characters

There are following characters in this document.

* Debug Target side
* Debugger side

### Debug Target side

Executes ` qemu-system-aarch64 ` on x86_64 host machine.

### Debugger side

x86_64 host machine.

## How to debug

### Debug Target side

Make UEFI application and run qemu.
Superuser required to setup Virtual NIC.

```
$ make
$ sudo ./run_qemu_aarch64.sh
```

` main.efi ` starts automaticaly after bootup and it waits connection from debugger.

### Debugger side

1. Open new termial
2. Launch ` aarch64-linux-gnu-gdb `
3. Load ` gdbscript.py `


```
$ aarch64-linux-gnu-gdb

...(snip)...

(gdb) source gdbscript.py
```

A ` gdbscript.py ` do the following:

1. Fetch image base address from UEFI application via TCP
2. Connect remote gdb server(QEMU).
3. Add debug symbols from ` main.so ` using image base address


Finally, change ` stop ` variable from ` TRUE ` to `FALSE` to start UEFI application.

```
(gdb) continue

[[press Ctrl+c to stop]]

(gdb) set variable stop = FALSE
(gdb) continue
``` 


## Reference

* https://wiki.osdev.org/Debugging_UEFI_applications_with_GDB
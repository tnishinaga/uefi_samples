import lldb
import shlex

def connect_qemu(debugger, command, result, internal_dict):
    argv = shlex.split(command)
    argc = len(argv)

    if argc < 2:
        # default value
        port = 1234
        host = "localhost"
    else:
        host = argv[1]
        port = argv[2]
    print("connect to {host}:{port}".format(host = host, port = port))
    debugger.HandleCommand("gdb-remote {host}:{port}".format(host = host, port = port))

    
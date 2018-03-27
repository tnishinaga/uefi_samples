import socket

c = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
c.connect(("192.168.20.20", 10000))

print(c.recv(10))

c.close()


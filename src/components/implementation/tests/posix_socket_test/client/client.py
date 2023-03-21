import socket
import time

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

t0 = time.time()

for i in range (1000):
    s.sendto(b"test data 1234567890", ("127.0.0.1", 8888))
    data, addr = s.recvfrom(1024)

t1 = time.time()

print("time " + str((t1-t0)/1000))
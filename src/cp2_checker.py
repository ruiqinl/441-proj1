#!/usr/bin/python

from socket import *
import sys
import random
import os
import time

s = socket(AF_INET, SOCK_STREAM)
s.connect(("127.0.0.1", 9999))


#s.send("GET /images/liso_header.png HTTP/1.1\r\nHos")
s.send("GET / HTTP/1.1\r\nHos")

s.send("t: localhost\r\nUser-Agent: Chr")

s.send("ome/29.0.1547.65\r\nContent-Length:8\r")

s.send("\n\r\n")

s.send("012")

s.send("ooxxabc");

buf = []
buf = s.recv(1024)
print buf

s.close()
				
print "cp2_checker_1 success!"


###

s = socket(AF_INET, SOCK_STREAM)
s.connect(("127.0.0.1", 9999))


#s.send("GET /images/liso_header.png HTTP/1.1\r\nHos")
s.send("METHODX  /unknown.html HTTP/1.0\r\nHos")

s.send("t: localhost\r\nUser-Agent: Chr")

s.send("ome/29.0.1547.65\r\nContent-Length:8\r")

s.send("\n\r\n")

s.send("012")

s.send("ooxxabc");

buf = []
buf = s.recv(1024)
print buf

s.close()
				
print "cp2_checker_2 success!"

###

s = socket(AF_INET, SOCK_STREAM)
s.connect(("127.0.0.1", 9999))


random_len = random.randrange(1, 1024)
random_string = os.urandom(random_len)

s.send(random_string)

buf = []
buf = s.recv(1024)
print buf

s.close()
				
print "cp2_checker_2 success!"

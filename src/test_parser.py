#!/usr/bin/python

from socket import *
import sys
import random
import os
import time

s = socket(AF_INET, SOCK_STREAM)
s.connect(("127.0.0.1", 9999))
#s.connect(("www.google.com", 80))

s.send("GET / HTTP/1.1\r\nHos")
time.sleep(1)
s.send("t: localhost\r\nUser-Agent: Chr")
time.sleep(1)
s.send("ome/29.0.1547.65\r\nContent-Length:8\r")
time.sleep(1)
s.send("\n\r\n")
time.sleep(1)
s.send("012")
time.sleep(1)
s.send("ooxxabc");

buf = []
buf = s.recv(1024)
print buf

s.close()
				
print "test_parser success!"

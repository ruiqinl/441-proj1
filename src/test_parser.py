#!/usr/bin/python

from socket import *
import sys
import random
import os
import time

s = socket(AF_INET, SOCK_STREAM)
s.connect(("127.0.0.1", 9999))
#s.connect(("www.google.com", 80))

s.send("GET /style.css HTTP/1.1\r\n\r\n")


buf = []
buf = s.recv(1024)
print buf

s.close()
				
print "test_parser success!"

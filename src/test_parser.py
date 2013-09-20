#!/usr/bin/python

from socket import *
import sys
import random
import os

s = socket(AF_INET, SOCK_STREAM)
s.connect(("127.0.0.1", 9999))

s.send("GET /ooxxuri HTTP/1.1\r\nHost: www.google.com\r\nUser-Agent: Chrome/29.0.1547.65\r\n\r\n")

buf = []
buf = s.recv(1024)

s.close()
				
print "test_parser success!"

#!/usr/bin/python

from socket import *
import sys
import random
import os
import time

s = socket(AF_INET, SOCK_STREAM)
s.connect(("127.0.0.1", 9999))


s.send("POST /cgi/cgi_dumper.py?action=opensearch&search=HT&namespace=0 HTTP/1.1\r\nConnection:keep-alive\r\nContent-Length:9\r\n\r\npost_body")


buf = []
buf = s.recv(1024)
print buf

#s.close()
				
print "cp2_checker_1 success!"


################################################################################
# Makefile                                                                     #
#                                                                              #
# Description: This file contains the make rules for Recitation 1.             #
#                                                                              #
# Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                          #
#          Wolf Richter <wolf@cs.cmu.edu>                                      #
#                                                                              #
################################################################################

default: lisod echo_client

lisod: lisod.c helper.c http_parser.c http_replyer.c helper.h cgi_parser.c
	gcc lisod.c helper.c http_parser.c http_replyer.c cgi_parser.c -o lisod -Wall -lssl

echo_client: echo_client.c helper.c http_replyer.c
	gcc echo_client.c helper.c http_replyer.c -lssl -o echo_client -Wall 

clean:
	rm -rf lisod echo_client *~

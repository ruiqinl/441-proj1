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

lisod: lisod.c helper.c http_parser.c
	gcc lisod.c helper.c http_parser.c -o lisod -Wall -Werror

echo_client: echo_client.c helper.c
	gcc echo_client.c helper.c -o echo_client -Wall -Werror

clean:
	rm -rf lisod echo_client *~

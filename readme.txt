################################################################################
# README                                                                       #
#                                                                              #
# Description: This file serves as a README and documentation for CP1 starter. #
#                                                                              #
#                                                                              #
################################################################################




[TOC-1] Table of Contents
--------------------------------------------------------------------------------

        [TOC-1] Table of Contents
        [DES-2] Description of Files
        [RUN-3] How to Run




[DES-2] Description of Files
--------------------------------------------------------------------------------

Here is a listing of all files associated with Recitation 1 and what their'
purpose is:

                    .../readme.txt              - Current document 
		     .../replay.out		   - File containing input to send 
						  to the server testing the implementation
		     .../replay.test		   - File containing expected server output
						  matching input given in replay.test
		    .../vulnerabilities.txt     - File containing documentation of at 
						  one vulnerability I identify at each
						  stage
                    .../src/echo_client.c       - Simple echo network client
                    .../src/echo_server.c       - Simple echo network server
                    .../src/Makefile            - Contains rules for make
                    .../src/cp1_checker.py      - Python test script for CP1
		    .../src/helper.c		- Define structure buffer and associated 
						  functions
		    .../src/helper.h		- Header file of helper.c




[RUN-3] How to Run
--------------------------------------------------------------------------------

Building and executing the echo code should be very simple:

                    cd src
                    make
                    ./echo_server &
                    ./echo_client 127.0.0.1 9999

This should allow you to input characters on stdin which will be sent to the
echo server.  The echo server has a hard-coded serving port of 9999.  Any input
characters to stdin will be flushed to the server on return, and any bytes
received from the server will appear on stdout.

The test Python script takes a series of arguments and can be run as:

                    cd src
                    make
                    ./echo_server &
                    ./cp1_checker 127.0.0.1 9999 1000 10 2048 500

with arguments as such:

                    <ip> <port> <# trials> <# writes and reads per trial> \
                    <max # bytes to write at a time> <# connections> 

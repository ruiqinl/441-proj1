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
		    .../tests.txt		- File containing documentation of your
						 test cases and any known issues you have
                    .../src/echo_client.c       - Simple echo network client
                    .../src/echo_server.c       - Simple echo network server
                    .../src/Makefile            - Contains rules for make
                    .../src/cp1_checker.py      - Python test script for CP1
		    .../src/helper.c		- Define structure buffer and associated 
						  functions
		   .../src/cp2_checker.py     - Python test script for CP2
		   .../src/http_parser.c	- Define methods to receive, parse and
						 respond http request
		   .../src/http_parser.h	- Header file of http_parser.c
		    .../src/helper.h		- Header file of helper.c




[RUN-3] How to Run
--------------------------------------------------------------------------------

Building and executing the echo code should be very simple:

                    cd src
                    make
                    ./lisod 9999 &
		    ./cp2_checker.py

This tests the liso with Python script cp2_checker.py

You can also just connect to lisod via Sarafi(Chrome fails, fix it later).







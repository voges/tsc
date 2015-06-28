#!/bin/bash

gcc -c arithcodec.c
ar r libarithcodec.a arithcodec.o
ranlib libarithcodec.a


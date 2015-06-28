#!/bin/bash

gcc -c ricecodec.c
ar r libricecodec.a ricecodec.o
ranlib libricecodec.a


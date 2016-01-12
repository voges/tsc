#!/bin/sh
set -x # echo on
cd libtnt
make static
cd ..
cd src
make

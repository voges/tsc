#!/bin/bash
mkdir libosro-1.0_install
cd libosro-1.0
make
make install prefix=../libosro-1.0_install/
cd ..
cd src
make clean
make
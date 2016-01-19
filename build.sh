#!/bin/bash

cd libosro-1.1
make
mkdir ../libosro-1.1_install
make install prefix=../libosro-1.1_install
cd ..
cd src
make

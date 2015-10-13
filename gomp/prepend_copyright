#!/bin/bash

while [ $# -gt 0 ]; do
    if [ -f $1 ]; then
        if [ ! -e $1.tmp ]; then
            cp $1 $1.tmp
            cat gplv3.txt $1.tmp > $1
            echo "Prepended GPL license to: $1"
            rm $1.tmp
            cp $1 $1.tmp
            cat copyright.txt $1.tmp > $1
            echo "Prependen copyright to: $1"
            rm $1.tmp
        else
            echo "Skipping: $1 ($1.tmp already exists)"
        fi
    else
        echo "Skipping: $1 (is not a file)"
    fi
    shift
done


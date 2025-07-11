#!/bin/bash

gcc file2.c -o file2 -lpthread
if [ $? -ne 0 ]; then
    echo "failed"
    exit 1
fi

./file2

rm -f file2

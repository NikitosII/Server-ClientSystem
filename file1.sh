#!/bin/bash

gcc file1.c -o file1 -lpthread
if [ $? -ne 0 ]; then 
    echo "failed"
    exit 1
fi

./file1

rm -f file1

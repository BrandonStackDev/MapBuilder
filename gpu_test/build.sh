#!/bin/bash

echo "start -> this is an experiment to test if my rpi5 can do gpu instancing. building test ..."
gcc test.c -o test -lraylib -lX11 -lGL -lm -lpthread -ldl -lrt
gcc test2.c -o works -lraylib -lX11 -lGL -lm -lpthread -ldl -lrt
gcc test3.c -o still -lraylib -lX11 -lGL -lm -lpthread -ldl -lrt
echo "!"
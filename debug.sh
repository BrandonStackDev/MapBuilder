#!/bin/bash

echo "start -> main (create)"
gcc -g -O0 main.c -o create -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

echo "start -> preview (play)"
gcc -g -O0 preview.c -o play -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

echo "start -> study (lod)"
gcc -g -O0 study.c -o lod -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

echo "start -> rocks (rock)"
gcc -g -O0 rocks.c -o rock -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

#!/bin/bash

echo "start -> main (create)"
gcc main.c -o create -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
echo "start -> preview (play)"
gcc preview.c -o play -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
echo "start -> study (lod)"
gcc study.c -o lod -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
echo "start -> rocks (rock)"
gcc rocks.c -o rock -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
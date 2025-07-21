#!/bin/bash

echo "start -> main (create)"
gcc main.c -o create -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
echo "start -> preview (play)"
gcc preview.c -o play -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
echo "start -> study (lod)"
gcc study.c -o lod -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
echo "start -> rocks (rock)"
gcc rocks.c -o rock -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
echo "start -> model_test (model_test)"
gcc model_test.c -o model_test -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
echo "start -> validate_tiles (validate_tiles)"
gcc validate_tiles.c -o validate_tiles -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

#test program for isntanced meshes
#gcc test.c -o test -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
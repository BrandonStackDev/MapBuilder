#!/bin/bash

#LDFLAGS="-lraylib -lGLESv2 -lm -lpthread -ldl -lrt -lX11" # - for lGLESv2
LDFLAGS="-lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -lusb-1.0" 

echo "start -> main (create)"
gcc main.c -o create $LDFLAGS
echo "start -> preview (play)"
gcc preview.c -o play $LDFLAGS
echo "start -> study (lod)"
gcc study.c -o lod $LDFLAGS
echo "start -> model_test (model_test)"
gcc model_test.c -o model_test $LDFLAGS
echo "start -> validate_tiles (validate_tiles)"
gcc validate_tiles.c -o validate_tiles $LDFLAGS
echo "start -> grass (grass)"
gcc grass.c -o grass $LDFLAGS

#test program for isntanced meshes
#gcc test.c -o test -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
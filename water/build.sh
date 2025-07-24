#!/bin/bash

#LDFLAGS="-lraylib -lGLESv2 -lm -lpthread -ldl -lrt -lX11" # - for lGLESv2
LDFLAGS="-lraylib -lGL -lm -lpthread -ldl -lrt -lX11" 

echo "start -> surfs up! .... wipe out!!!! (water)"
gcc water.c -o water $LDFLAGS

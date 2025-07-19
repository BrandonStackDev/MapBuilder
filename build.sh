#!/bin/bash

gcc main.c -o create -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
gcc preview.c -o play -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
gcc study.c -o lod -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
gcc rocks.c -o rock -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
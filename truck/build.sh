#!/bin/bash

LDFLAGS="-lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -lusb-1.0" 
echo "start -> truck (truck) [vroom vroom!!!!!]"
gcc truck.c -o truck $LDFLAGS
echo "start -> control (control) [..?]"
gcc control.c -o control $LDFLAGS
echo "start -> ps5 (ps5) [..?]"
gcc ps5.c -o ps5 $LDFLAGS
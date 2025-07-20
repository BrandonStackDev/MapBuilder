#!/bin/bash

echo "find all tiles with a $1"
find ./map -type f -name "tile_$1_64.obj" | awk -F'[/_]' '{ printf "chunk_%s_%s -> tile_%s_%s\n", $4, $5, $8, $9 }' | sort


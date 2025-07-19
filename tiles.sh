#!/bin/bash

find ./map -type f -name "tile_64.obj" | awk -F'[/_]' '{ printf "chunk_%s_%s -> tile_%s_%s\n", $4, $5, $8, $9 }' | sort

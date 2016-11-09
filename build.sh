#!/bin/bash
# usage
# --------
# build: sh build.sh synth_samples_sdl2_1
# build and run: sh build.sh synth_samples_sdl2_1 run
name=$1
param=$2
if [[ -n "$name" ]]; then
    gcc -F/Library/Frameworks -framework SDL2 src/"$name".c -o bin/"$name"
    if [[ "$param" == "run" ]]; then
	./bin/"$name"
    fi
else
    echo "error: enter name of c file to compile."
fi

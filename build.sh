#!/bin/sh -xe
cc -Wall -Wextra -std=c11 -pedantic -o musik src/main.c -lSDL2 -lSDL2_mixer

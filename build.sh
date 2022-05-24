#!/bin/sh -xe
cc -Wall -Wextra -std=c11 -pedantic -o smm main.c -lpulse -lpulse-simple

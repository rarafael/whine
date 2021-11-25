#!/bin/sh

set -xe

CFLAGS="-Wall -Wextra -Wshadow -Werror -std=c11 -pedantic -ggdb `pkg-config --cflags sdl2`"
LIBS="`pkg-config --libs sdl2` -lm"

cc $CFLAGS -o whine main.c $LIBS

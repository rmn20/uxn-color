#!/bin/sh -e

clang-format -i circle128.c
rm -f circle128
cc -lm circle128 -o circle128.c
circle128 128


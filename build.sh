#!/bin/bash

gcc src/*.c -o bin/yarn -DYARN_STANDALONE -O3 -std=c99 -pedantic -Wall -Wextra \
        -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes "$@"

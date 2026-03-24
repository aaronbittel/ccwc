#!/usr/bin/env bash

set -xe

CFLAGS="-Wall -Wextra -pedantic -ggdb"

gcc $CFLAGS -o ccwc main.c

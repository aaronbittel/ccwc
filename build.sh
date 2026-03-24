#!/usr/bin/env bash

set -xe

CFLAGS="-Wall -Wextra -pedantic"

gcc $CFLAGS -o ccwc main.c

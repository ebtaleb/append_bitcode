#!/bin/bash

diff -u <(otool -hl $1) <(otool -hl $2) || exit 0

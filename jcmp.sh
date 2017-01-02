#!/bin/bash

diff -u <(jtool -l $1) <(jtool -l $2) || exit 0

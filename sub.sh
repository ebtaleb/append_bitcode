#!/bin/bash

set -x
find . -type f -name '*.original' | while read f; do cp $f "${f%.original}"; done

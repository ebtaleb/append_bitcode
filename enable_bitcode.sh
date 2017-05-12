#!/bin/bash

#for file in "$(ag -l --ignore '*.sh' --ignore '*.*.bak' 'ENABLE_BITCODE = NO')"
#do
    #echo "replacing in "$file""
#done

#grep -rnw '.' --include \*.pbxproj --include \*.xcconfig -e "ENABLE_BITCODE = NO"
#find . -type f -name '*.xcconfig' -o -name '*.pbxproj'

#find ./ -maxdepth 3 -mindepth 3 -type d -iname "*.xcodeproj" -print0 |\
#find . -type f -name '*.xcconfig' -o -name '*.pbxproj' -print0 |\
grep -lrnw '.' --include \*.pbxproj --include \*.xcconfig -e "ENABLE_BITCODE = NO" |\
    while read file; do
        echo "replacing in $file"
        sed -i .bak 's/BITCODE = NO/BITCODE = YES/' "$file"
    done


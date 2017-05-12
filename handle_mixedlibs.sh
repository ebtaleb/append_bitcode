#!/bin/bash
set -x
## Stubs the arch-specific libraries that do not contain bitcode. (usually x86)

library=$1
libname=${library%.*}
parentdir="$(dirname "$library")"
archs=`lipo -info $library | sed "s/Architectures in the fat file: $library are: //g"`
BAKEXTN=".bak"

if [ ! -f "$library$BAKEXTN" ]
then
  echo "Backing up $library"
  cp $library "$library$BAKEXTN"
fi

for arch in ${archs[*]}
do
  arch_lib=${libname}_${arch}
  lipo -thin $arch $library -o ${arch_lib}

  otool -hl ${arch_lib} | grep __bitcode > /dev/null

  if [ $? -ne 0 ]; then
    echo "$arch doesnt contain bitcode"
    stub_lib=`ixguard-stubs -i=${arch_lib} -arch=$arch | tail -n -1`
    if [ ! -z "$stub_lib" ]; then
      lipo -replace $arch $stub_lib $library -o $library
    fi
  fi

  rm $arch_lib
done

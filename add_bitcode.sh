#!/bin/bash

if [ "$#" -ne 1 ] || ! [ -e "$1" ]; then
  echo "Usage: $0 LIBRARY" >&2
  exit 1
fi

library=$1
insert_source="remake.c"
insert="../a.out"
data_path="../test"
data_path2="../test2"

clang $insert_source

archs=`lipo -info $library | sed "s/Architectures in the fat file: $library are: //g"`
#echo $archs

libname=${library%.*}

otool -hl $library | grep __bitcode > /dev/null

if [ $? -eq 0 ]; then
    echo "macho binary already contain bitcode"
    exit 1
fi

# for each architectures in fat binary
for arch in ${archs[*]}
do
    arch_lib=${libname}_${arch}
    if [ ! -d "$arch_lib" ]; then
        mkdir $arch_lib

        # extract thin binary
        lipo -thin $arch $library -o $arch_lib/${arch_lib}.a
        cd $arch_lib

        # extract lib archive
        ar -x ${arch_lib}.a

        # generate list of object files affected
        objfiles=(*.o)

        # for each object file found in archive
        for obj in ${objfiles[*]}
        do
            # apply bitcode section addition 
            echo "[+] $arch : adding bc to $obj"
            #$insert $data_path $data_path2 $obj $obj.new --candidates "huey,dewey,louie"
            $insert --inplace $data_path $data_path2 $obj --candidates "huey,dewey,louie"
            echo " "
        done

        arch_files=`ar -t ${arch_lib}.a`

        # repackage library using libtool or lipo
        ar cr ../${arch_lib}.a ${arch_files}

        cd ..

    fi
done

lipo *.a -create -output $libname
rm *.a
rm a.out

cleanUp() {
    echo "[+] Cleaning..."
    for arch in ${archs[*]}
    do
        local curr_arch=${libname}_${arch}
        if [ -d "$curr_arch" ]; then
            echo "deleting $curr_arch"
            rm -rf $curr_arch
        fi
    done

    
    if [ -e "$libname" ]; then
        rm $libname
    fi
}

while true; do
    read -p "Do you want to clean everything? (yn) " yn
    case $yn in
        [Yy]* ) cleanUp; break;;
        [Nn]* ) break;;
        * ) echo "Please answer yes or no.";;
    esac
done

if [ -e "$libname" ]; then

    cd /Users/elias/Desktop/hniosreader
    export IPHONEOS_DEPLOYMENT_TARGET=8.1
    export PATH="/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin:/Applications/Xcode.app/Contents/Developer/usr/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin"
    /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang -arch armv7 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS10.2.sdk -L/Users/elias/Desktop/hniosreader/DerivedData/hn/Build/Intermediates/ArchiveIntermediates/hn/BuildProductsPath/Release-iphoneos -L/Users/elias/Desktop/hniosreader/DerivedData/hn/Build/Intermediates/ArchiveIntermediates/hn/BuildProductsPath/Release-iphoneos/Appirater -L/Users/elias/Desktop/hniosreader/DerivedData/hn/Build/Intermediates/ArchiveIntermediates/hn/BuildProductsPath/Release-iphoneos/Colours -L/Users/elias/Desktop/hniosreader/DerivedData/hn/Build/Intermediates/ArchiveIntermediates/hn/BuildProductsPath/Release-iphoneos/MBProgressHUD -L/Users/elias/Desktop/hniosreader/DerivedData/hn/Build/Intermediates/ArchiveIntermediates/hn/BuildProductsPath/Release-iphoneos/NJKWebViewProgress -L/Users/elias/Desktop/hniosreader/DerivedData/hn/Build/Intermediates/ArchiveIntermediates/hn/BuildProductsPath/Release-iphoneos/SWRevealViewController -F/Users/elias/Desktop/hniosreader/DerivedData/hn/Build/Intermediates/ArchiveIntermediates/hn/BuildProductsPath/Release-iphoneos -F/Users/elias/Desktop/hniosreader/hn -F/Users/elias/Desktop/hniosreader -filelist /Users/elias/Desktop/hniosreader/DerivedData/hn/Build/Intermediates/ArchiveIntermediates/hn/IntermediateBuildFilesPath/hn.build/Release-iphoneos/hn.build/Objects-normal/armv7/hn.LinkFileList -Xlinker -rpath -Xlinker @executable_path/Frameworks -miphoneos-version-min=8.1 -dead_strip -Xlinker -object_path_lto -Xlinker /Users/elias/Desktop/hniosreader/DerivedData/hn/Build/Intermediates/ArchiveIntermediates/hn/IntermediateBuildFilesPath/hn.build/Release-iphoneos/hn.build/Objects-normal/armv7/hn_lto.o -fembed-bitcode -Xlinker -bitcode_verify -Xlinker -bitcode_hide_symbols -Xlinker -bitcode_symbol_map -Xlinker /Users/elias/Desktop/hniosreader/DerivedData/hn/Build/Intermediates/ArchiveIntermediates/hn/BuildProductsPath/Release-iphoneos -fobjc-arc -fobjc-link-runtime -ObjC -lAppirater -lColours -lMBProgressHUD -lNJKWebViewProgress -lSWRevealViewController -framework CFNetwork -framework CoreGraphics -framework SystemConfiguration -weak_framework StoreKit -ObjC -framework SystemConfiguration -framework Security -framework CFNetwork -framework Firebase -lc++ -licucore -framework BuddyBuildSDK -framework AssetsLibrary -framework CoreText -framework CoreTelephony -lPods-hn -Xlinker -dependency_info -Xlinker /Users/elias/Desktop/hniosreader/DerivedData/hn/Build/Intermediates/ArchiveIntermediates/hn/IntermediateBuildFilesPath/hn.build/Release-iphoneos/hn.build/Objects-normal/armv7/hn_dependency_info.dat -o /Users/elias/Desktop/hniosreader/DerivedData/hn/Build/Intermediates/ArchiveIntermediates/hn/IntermediateBuildFilesPath/hn.build/Release-iphoneos/hn.build/Objects-normal/armv7/hn

    cd -
    #:
fi




#!/bin/bash
# WINDOWS PACKAGING SCRIPT FOR NAEV
# Requires NSIS, and python3-pip to be installed
#
# This script should be run after compiling Naev
# It detects the current environment, and builds the appropriate NSIS installer
# into the root naev directory.
#

# Checks if argument(s) are valid

if [[ $1 == "--nightly" ]]; then
    echo "Building for nightly release"
    NIGHTLY=true
    # Get Formatted Date
    BUILD_DATE="$(date +%m_%d_%Y)"
elif [[ $1 == "" ]]; then
    echo "No arguments passed, assuming normal release"
    NIGHTLY=false
elif [[ $1 != "--nightly" ]]; then
    echo "Please use argument --nightly if you are building this as a nightly build"
    exit -1
else
    echo "Something went wrong."
    exit -1
fi

# Check if we are running in the right place

if [[ ! -f "naev.6" ]]; then
    echo "Please run from Naev root directory."
    exit -1
fi

# Rudementary way of detecting which environment we are packaging.. 
# It works, and it should remain working until msys changes their naming scheme

if [[ $PATH == *"mingw32"* ]]; then
    echo "Detected MinGW32 environment"
    ARCH="32"
elif [[ $PATH == *"mingw64"* ]]; then
    echo "Detected MinGW64 environment"
    ARCH="64"
else
    echo "Welp, I don't know what environment this is... Make sure you are running this in an MSYS2 MinGW environment"
    exit -1
fi

VERSION="$(cat $(pwd)/VERSION)"
# Get version, negative minors mean betas
if [[ -n $(echo "$VERSION" | grep "-") ]]; then
    BASEVER=$(echo "$VERSION" | sed 's/\.-.*//')
    BETAVER=$(echo "$VERSION" | sed 's/.*-//')
    VERSION="$BASEVER.0-beta$BETAVER"
else
    echo "could not find VERSION file"
    exit -1
fi

# Download and Install mingw-ldd
echo "Update pip"
pip3 install --upgrade pip

echo "Install mingw-ldd script"
pip3 install mingw-ldd

# Download Inetc
# This is the Plugin that handles downloading NData, this needed to be 
# changed as NSISdl does not support secure downloads

echo "Creating Temp Folder"
mkdir -p ~/temp

echo "Downloading Inetc release"
wget https://nsis.sourceforge.io/mediawiki/images/c/c9/Inetc.zip -P ~/temp

echo "Decompressing Inetc release"
unzip ~/temp/Inetc.zip -d ~/temp

# Install Inetc

echo "Installing Inetc"
if [[ $ARCH == "32" ]]; then
    cp -r ~/temp/Plugins/x86-unicode/* /mingw32/share/nsis/Plugins/unicode
    echo "Cleaning up temp folder"
    rm -rf ~/temp
elif [[ $ARCH == "64" ]]; then
    cp -r ~/temp/Plugins/amd64-unicode/* /mingw64/share/nsis/Plugins/unicode
    echo "Cleaning up temp folder"
    rm -rf ~/temp
else
    echo "I'm afraid I can't do that Dave..."
    echo "Cleaning up temp folder"
    rm -rf ~/temp
    exit -1
fi

# Move compiled binary to staging folder.

echo "creating staging area"
mkdir -p extras/windows/installer/bin

# Collect DLLs
 
if [[ $ARCH == "32" ]]; then
for fn in `mingw-ldd naev.exe --dll-lookup-dirs /mingw32/bin | grep -i "mingw32" | cut -f1 -d"/" --complement`; do
    fp="/"$fn
    echo "copying $fp to staging area"
    cp $fp extras/windows/installer/bin
done
elif [[ $ARCH == "64" ]]; then
for fn in `mingw-ldd naev.exe --dll-lookup-dirs /mingw64/bin | grep -i "mingw64" | cut -f1 -d"/" --complement`; do
    fp="/"$fn
    echo "copying $fp to staging area"
    cp $fp extras/windows/installer/bin
done
else
    echo "Aw, man, I shot Marvin in the face..."
    echo "Something went wrong while looking for DLLs to stage."
    exit -1
fi

echo "copying naev binary to staging area"
if [[ $NIGHTLY == true ]]; then
cp src/naev.exe extras/windows/installer/bin/naev-$VERSION-$BUILD_DATE-win$ARCH.exe
elif [[ $NIGHTLY == false ]]; then
cp src/naev.exe extras/windows/installer/bin/naev-$VERSION-win$ARCH.exe
else
    echo "Cannot think of another movie quote."
    echo "Something went wrong while copying binary to staging area."
    exit -1
fi

# Build installer

if [[ $NIGHTLY == true ]]; then
makensis -DVERSION=$BASEVER.0 -DVERSION_SUFFIX=-beta$BETAVER-$BUILD_DATE -DARCH=$ARCH extras/windows/installer/naev-nightly.nsi

# Move installer to root directory
mv extras/windows/installer/naev-$VERSION-$BUILD_DATE-win$ARCH.exe naev-win$ARCH.exe

elif [[ $NIGHTLY == false ]]; then
makensis -DVERSION=$BASEVER.0 -DVERSION_SUFFIX=-beta$BETAVER -DARCH=$ARCH extras/windows/installer/naev.nsi

# Move installer to root directory
mv extras/windows/installer/naev-$VERSION-win$ARCH.exe naev-win$ARCH.exe
else
    echo "Cannot think of another movie quote.. again."
    echo "Something went wrong.."
    exit -1
fi

echo "Successfully built Windows Installer for win$ARCH"

# Package zip

if [[ $NIGHTLY == true ]]; then
cd extras/windows/installer/bin
zip ../../../../naev-win$ARCH.zip *.*
cd ../../../../

elif [[ $NIGHTLY == false ]]; then
cd extras/windows/installer/bin
zip ../../../../naev-win$ARCH.zip *.*
cd ../../../../

else
    echo "Cannot think of another movie quote.. again."
    echo "Something went wrong.."
    exit -1
fi

echo "Successfully packaged zipped folder for win$ARCH"

echo "Cleaning up staging area"
rm -rf extras/windows/installer/bin

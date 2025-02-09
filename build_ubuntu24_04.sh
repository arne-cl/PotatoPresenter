#!/bin/bash
set -euo pipefail

# Check if running with root privileges
if [[ $EUID -ne 0 ]]; then
    echo "This script must be run as root. Restarting with sudo..."
    exec sudo /bin/bash "$0" "$@"
    exit $?
fi

# Install dependencies
export DEBIAN_FRONTEND=noninteractive
echo "Installing dependencies..."
apt-get update
apt-get install -y \
    build-essential \
    cmake \
    g++-11 \
    gettext \
    git \
    libkf5syntaxhighlighting-dev \
    libkf5texteditor-dev \
    pkg-config \
    qtbase5-dev \
    qtdeclarative5-dev \
    libqt5svg5-dev \
    texlive-fonts-extra \
    texlive-latex-extra \
    texlive-science \
    unzip \
    uuid-dev \
    extra-cmake-modules

# Store the original directory
ORIGINAL_DIR=$(pwd)

# Install ANTLR4
echo "Installing ANTLR4..."
mkdir -p /tmp/antlr4
cd /tmp/antlr4
wget https://www.antlr.org/download/antlr4-cpp-runtime-4.9.3-source.zip
unzip antlr4-cpp-runtime-4.9.3-source.zip
env CXX=g++-11 cmake . -DANTLR4_INSTALL=ON -DWITH_DEMO=False
make install

# Build Potato Presenter
echo "Building Potato Presenter..."
cd "$ORIGINAL_DIR"

mkdir -p build
cd build
rm -rf *

# Configure with CMake
cmake .. \
    -DCMAKE_CXX_COMPILER=g++-11 \
    -DCMAKE_PREFIX_PATH="/usr/lib/x86_64-linux-gnu/cmake;/usr/local/lib/cmake/antlr4" \
    -DCMAKE_INCLUDE_PATH="/usr/include/KF5" \
    -DCMAKE_CXX_FLAGS="-I/usr/include/KF5 -I/usr/include/KF5/KSyntaxHighlighting/KSyntaxHighlighting" \
    -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc)

echo "Build completed. The binary is located in $(pwd)/PotatoPresenter"

# Cleanup
rm -rf /tmp/antlr4

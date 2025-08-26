#!/bin/bash

set -e

BUILD_TYPE="Release"
BUILD_DIR="build"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="Debug"
            BUILD_DIR="build-debug"
            shift
            ;;
        -r|--release)
            BUILD_TYPE="Release"
            BUILD_DIR="build"
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  -d, --debug    Build in debug mode (O0)"
            echo "  -r, --release  Build in release mode (default)"
            echo "  -h, --help     Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

echo "Building in $BUILD_TYPE mode..."

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure and build
cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" ..
make -j$(nproc)

echo "Build completed successfully in $BUILD_DIR/"
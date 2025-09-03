#!/bin/bash

set -e

BUILD_TYPE="Release"
BUILD_DIR="build"
TARGET="all"

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
        -p|--proxy)
            TARGET="all"
            BUILD_DIR="build-proxy"
            shift
            ;;
        --server-only)
            TARGET="http2-boost-server"
            shift
            ;;
        --proxy-only)
            TARGET="proxy_example"
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  -d, --debug      Build in debug mode (O0)"
            echo "  -r, --release    Build in release mode (default)"
            echo "  -p, --proxy      Build all including proxy components"
            echo "  --server-only    Build only the main HTTP/2 server"
            echo "  --proxy-only     Build only the proxy example client"
            echo "  -h, --help       Show this help message"
            echo ""
            echo "Available executables:"
            echo "  http2-boost-server - Main HTTP/2 server with proxy capabilities"
            echo "  forwarding_client  - Forwarding client for tunnel management"
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

if [[ "$TARGET" == "all" ]]; then
    make -j$(nproc)
    echo "Build completed successfully in $BUILD_DIR/"
    echo "Available executables:"
    if [[ -f "http2-boost-server" ]]; then
        echo "  - http2-boost-server (Main HTTP/2 server with proxy)"
    fi
    if [[ -f "forwarding_client" ]]; then
        echo "  - forwarding_client (Tunnel forwarding)"
    fi
else
    make -j$(nproc) "$TARGET"
    echo "Build completed successfully in $BUILD_DIR/"
    echo "Built executable: $TARGET"
fi
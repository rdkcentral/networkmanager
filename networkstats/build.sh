#!/bin/bash
# Build script for ThunderJsonRPCProvider Test Application
# Copyright 2025 RDK Management

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "========================================"
echo "ThunderJsonRPCProvider Test Build"
echo "========================================"
echo ""

# Check for required dependencies
check_dependency() {
    local lib=$1
    local header=$2
    
    if [ -f "/usr/include/$header" ] || [ -f "/usr/local/include/$header" ] || [ -f "/opt/homebrew/include/$header" ]; then
        echo "✓ Found: $lib"
        return 0
    else
        echo "✗ Missing: $lib"
        return 1
    fi
}

echo "Checking dependencies..."
all_deps_found=true

if ! check_dependency "jsoncpp" "json/json.h"; then
    all_deps_found=false
fi

if ! check_dependency "libjsonrpccpp" "jsonrpccpp/client.h"; then
    all_deps_found=false
fi

echo ""

if [ "$all_deps_found" = false ]; then
    echo "❌ Missing required dependencies!"
    echo ""
    echo "To install on Ubuntu/Debian:"
    echo "  sudo apt-get install libjsonrpccpp-dev libjsoncpp-dev"
    echo ""
    echo "To install on macOS:"
    echo "  brew install jsoncpp libjson-rpc-cpp"
    echo ""
    exit 1
fi

echo "✓ All dependencies found"
echo ""
echo "Building test application..."

# Compile
g++ -DTEST_MAIN -std=c++11 -Wall -Wno-deprecated-declarations -g \
    -I. -I/usr/include -I/usr/local/include -I/opt/homebrew/include \
    ThunderJsonRPCProvider.cpp \
    -o thunder-jsonrpc-provider-test \
    -L/usr/lib -L/usr/lib/x86_64-linux-gnu -L/usr/local/lib -L/opt/homebrew/lib \
    -ljsonrpccpp-client -ljsonrpccpp-common -ljsoncpp

if [ $? -eq 0 ]; then
    echo "✓ Build successful!"
    echo ""
    echo "Test application: ./thunder-jsonrpc-provider-test"
    echo ""
    echo "To run the test:"
    echo "  ./thunder-jsonrpc-provider-test"
    echo ""
else
    echo "❌ Build failed!"
    exit 1
fi

#!/bin/bash

# Copyright (C) 2024 THL A29 Limited, a Tencent company.
# BQLOG is licensed under the Apache License, Version 2.0.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NODEJS_WRAPPER_DIR="$SCRIPT_DIR/../../../wrapper/nodejs"
BUILD_DIR="$SCRIPT_DIR/build"

echo "Building BqLog Node.js wrapper..."
echo "Script directory: $SCRIPT_DIR"
echo "Node.js wrapper directory: $NODEJS_WRAPPER_DIR"

# Check if Node.js is available
if ! command -v node &> /dev/null; then
    echo "Error: Node.js is not installed or not in PATH"
    exit 1
fi

# Check if npm is available
if ! command -v npm &> /dev/null; then
    echo "Error: npm is not installed or not in PATH"
    exit 1
fi

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"

# Change to Node.js wrapper directory
cd "$NODEJS_WRAPPER_DIR"

echo "Installing Node.js dependencies..."
npm install

echo "Building native addon..."
npm run build

# Test the build
echo "Running basic tests..."
node test/test.js

echo "Node.js wrapper build completed successfully!"
echo "Built addon location: $NODEJS_WRAPPER_DIR/build/Release/bqlog.node"

# Copy to build directory for distribution
if [ -f "build/Release/bqlog.node" ]; then
    cp "build/Release/bqlog.node" "$BUILD_DIR/"
    echo "Copied addon to: $BUILD_DIR/bqlog.node"
fi

echo "Build completed!"

#!/bin/bash

echo "Building Ultimate 64 Viewer (Swift)..."

# Clean previous build
rm -rf .build

# Build the project
swift build -c release

if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo "Run with: swift run Ultimate64Viewer"
    echo "Or find the executable at: .build/release/Ultimate64Viewer"
else
    echo "Build failed!"
    exit 1
fi
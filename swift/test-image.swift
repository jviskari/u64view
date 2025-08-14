#!/usr/bin/env swift

import Foundation
import AppKit

// Test image creation with known data
func testImageCreation() {
    let width = 384
    let height = 272
    let bytesPerPixel = 4  // Use RGBA instead of RGB
    var testData = Data(count: width * height * bytesPerPixel)
    
    // Create a simple test pattern - red/green/blue stripes with alpha
    for y in 0..<height {
        for x in 0..<width {
            let pos = (y * width + x) * bytesPerPixel
            if y < height / 3 {
                testData[pos] = 255     // Red
                testData[pos + 1] = 0   // Green
                testData[pos + 2] = 0   // Blue
                testData[pos + 3] = 255 // Alpha
            } else if y < 2 * height / 3 {
                testData[pos] = 0       // Red
                testData[pos + 1] = 255 // Green
                testData[pos + 2] = 0   // Blue
                testData[pos + 3] = 255 // Alpha
            } else {
                testData[pos] = 0       // Red
                testData[pos + 1] = 0   // Green
                testData[pos + 2] = 255 // Blue
                testData[pos + 3] = 255 // Alpha
            }
        }
    }
    
    print("Created test data: \(testData.count) bytes")
    
    // Create image with RGBA format
    let colorSpace = CGColorSpaceCreateDeviceRGB()
    let bitmapInfo = CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue)
    
    guard let context = CGContext(
        data: testData.withUnsafeMutableBytes { $0.baseAddress },
        width: width,
        height: height,
        bitsPerComponent: 8,
        bytesPerRow: width * bytesPerPixel,
        space: colorSpace,
        bitmapInfo: bitmapInfo.rawValue
    ) else {
        print("Failed to create CGContext")
        return
    }
    
    guard let cgImage = context.makeImage() else {
        print("Failed to create CGImage")
        return
    }
    
    let nsImage = NSImage(cgImage: cgImage, size: NSSize(width: width, height: height))
    print("Successfully created test NSImage: \(nsImage.size)")
    
    // Save to desktop for verification
    if let tiffData = nsImage.tiffRepresentation,
       let bitmapRep = NSBitmapImageRep(data: tiffData),
       let pngData = bitmapRep.representation(using: .png, properties: [:]) {
        let url = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Desktop/test-image.png")
        try? pngData.write(to: url)
        print("Saved test image to: \(url.path)")
    }
}

testImageCreation()
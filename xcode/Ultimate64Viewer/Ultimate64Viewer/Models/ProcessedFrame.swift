import Foundation
#if canImport(UIKit)
import UIKit
#elseif canImport(AppKit)
import AppKit
#endif

struct ProcessedFrame: Identifiable {
    let id = UUID()
    let number: UInt16
    let timestamp: Date
    let rgbData: Data
    let width: Int
    let height: Int
    
    init(number: UInt16, rgbData: Data, width: Int = 384, height: Int = 272) {
        self.number = number
        self.timestamp = Date()
        self.rgbData = rgbData
        self.width = width
        self.height = height
    }
    
    func createImage() -> Any? {
        guard rgbData.count == width * height * 3 else { return nil }
        
        var rgbaData = Data(count: width * height * 4)
        for i in 0..<(width * height) {
            let rgbIndex = i * 3
            let rgbaIndex = i * 4
            
            if rgbIndex + 2 < rgbData.count && rgbaIndex + 3 < rgbaData.count {
                rgbaData[rgbaIndex] = rgbData[rgbIndex]
                rgbaData[rgbaIndex + 1] = rgbData[rgbIndex + 1]
                rgbaData[rgbaIndex + 2] = rgbData[rgbIndex + 2]
                rgbaData[rgbaIndex + 3] = 255
            }
        }
        
        let colorSpace = CGColorSpaceCreateDeviceRGB()
        let bitmapInfo = CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue)
        
        guard let context = CGContext(
            data: rgbaData.withUnsafeMutableBytes { $0.baseAddress },
            width: width,
            height: height,
            bitsPerComponent: 8,
            bytesPerRow: width * 4,
            space: colorSpace,
            bitmapInfo: bitmapInfo.rawValue
        ) else { return nil }
        
        guard let cgImage = context.makeImage() else { return nil }
        
        #if canImport(UIKit)
        return UIImage(cgImage: cgImage)
        #elseif canImport(AppKit)
        return NSImage(cgImage: cgImage, size: NSSize(width: width, height: height))
        #endif
    }
}
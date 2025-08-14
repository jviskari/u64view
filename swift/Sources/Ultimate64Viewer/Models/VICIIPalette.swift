import AppKit

struct VICIIPalette {
    static let colors: [NSColor] = [
        NSColor(red: 0x00/255.0, green: 0x00/255.0, blue: 0x00/255.0, alpha: 1.0), // Black
        NSColor(red: 0xEF/255.0, green: 0xEF/255.0, blue: 0xEF/255.0, alpha: 1.0), // White
        NSColor(red: 0x8D/255.0, green: 0x2F/255.0, blue: 0x34/255.0, alpha: 1.0), // Red
        NSColor(red: 0x6A/255.0, green: 0xD4/255.0, blue: 0xCD/255.0, alpha: 1.0), // Cyan
        NSColor(red: 0x98/255.0, green: 0x35/255.0, blue: 0xA4/255.0, alpha: 1.0), // Purple
        NSColor(red: 0x4C/255.0, green: 0xB4/255.0, blue: 0x42/255.0, alpha: 1.0), // Green
        NSColor(red: 0x2C/255.0, green: 0x29/255.0, blue: 0xB1/255.0, alpha: 1.0), // Blue
        NSColor(red: 0xEF/255.0, green: 0xEF/255.0, blue: 0x5D/255.0, alpha: 1.0), // Yellow
        NSColor(red: 0x98/255.0, green: 0x4E/255.0, blue: 0x20/255.0, alpha: 1.0), // Orange
        NSColor(red: 0x5B/255.0, green: 0x38/255.0, blue: 0x00/255.0, alpha: 1.0), // Brown
        NSColor(red: 0xD1/255.0, green: 0x67/255.0, blue: 0x6D/255.0, alpha: 1.0), // Light Red
        NSColor(red: 0x4A/255.0, green: 0x4A/255.0, blue: 0x4A/255.0, alpha: 1.0), // Dark Grey
        NSColor(red: 0x7B/255.0, green: 0x7B/255.0, blue: 0x7B/255.0, alpha: 1.0), // Grey
        NSColor(red: 0x9F/255.0, green: 0xEF/255.0, blue: 0x93/255.0, alpha: 1.0), // Light Green
        NSColor(red: 0x6D/255.0, green: 0x6A/255.0, blue: 0xEF/255.0, alpha: 1.0), // Light Blue
        NSColor(red: 0xB2/255.0, green: 0xB2/255.0, blue: 0xB2/255.0, alpha: 1.0), // Light Grey
    ]
    
    static func getRGBComponents(for colorIndex: UInt8) -> (r: UInt8, g: UInt8, b: UInt8) {
        let index = Int(colorIndex) % colors.count
        let color = colors[index]
        
        var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
        color.getRed(&r, green: &g, blue: &b, alpha: &a)
        
        return (
            r: UInt8(r * 255),
            g: UInt8(g * 255),
            b: UInt8(b * 255)
        )
    }
}
import Foundation

class FrameProcessor {
    private var currentFrameNumber: UInt16 = 0
    private var packetsCollected: [Data] = []
    private let packetsPerFrame = 68
    private let frameWidth = 384
    private let frameHeight = 272
    
    private var frameStarted = false
    private let lock = NSLock()
    
    func processPacket(_ data: Data) -> ProcessedFrame? {
        lock.lock()
        defer { lock.unlock() }
        
        guard data.count >= 12 else { return nil }
        
        let header = PacketHeader(from: data)
        
        // Check if this is a new frame
        if !frameStarted {
            currentFrameNumber = header.frm
            packetsCollected.removeAll()
            frameStarted = true
        }
        
        if header.frm != currentFrameNumber {
            // Process the completed frame if we have enough packets
            var completedFrame: ProcessedFrame? = nil
            if packetsCollected.count == packetsPerFrame {
                completedFrame = assembleFrame(frameNumber: currentFrameNumber, packets: packetsCollected)
            }
            
            // Start new frame
            currentFrameNumber = header.frm
            packetsCollected.removeAll()
            frameStarted = true
            
            // Add packet data for new frame (skip 12-byte header)
            let pixelData = data.dropFirst(12)
            packetsCollected.append(Data(pixelData))
            
            return completedFrame
        }
        
        // Add packet data (skip 12-byte header)
        let pixelData = data.dropFirst(12)
        packetsCollected.append(Data(pixelData))
        
        // Check if frame is complete
        if packetsCollected.count == packetsPerFrame {
            let frame = assembleFrame(frameNumber: currentFrameNumber, packets: packetsCollected)
            packetsCollected.removeAll()
            frameStarted = false
            return frame
        }
        
        return nil
    }
    
    private func assembleFrame(frameNumber: UInt16, packets: [Data]) -> ProcessedFrame {
        var rgbData = Data(count: frameWidth * frameHeight * 3)
        
        var y = 0
        
        for packet in packets {
            guard y < frameHeight else { break }
            
            var pixelIndex = 0
            
            // Each packet contains 4 lines of data
            for _ in 0..<4 {
                guard y < frameHeight else { break }
                
                // Each line has 192 bytes (384 pixels / 2 pixels per byte)
                for x in stride(from: 0, to: min(192, frameWidth / 2), by: 1) {
                    guard pixelIndex < packet.count else { break }
                    
                    let byte = packet[pixelIndex]
                    pixelIndex += 1
                    
                    // Extract two 4-bit pixels
                    let pixel1 = byte & 0x0F
                    let pixel2 = (byte & 0xF0) >> 4
                    
                    // Convert to RGB using VIC-II palette
                    let color1 = VICIIPalette.getRGBComponents(for: pixel1)
                    let color2 = VICIIPalette.getRGBComponents(for: pixel2)
                    
                    // Calculate positions in RGB buffer
                    let pos1 = (y * frameWidth + x * 2) * 3
                    let pos2 = (y * frameWidth + x * 2 + 1) * 3
                    
                    // Set pixel 1
                    if pos1 + 2 < rgbData.count {
                        rgbData[pos1] = color1.r
                        rgbData[pos1 + 1] = color1.g
                        rgbData[pos1 + 2] = color1.b
                    }
                    
                    // Set pixel 2
                    if pos2 + 2 < rgbData.count {
                        rgbData[pos2] = color2.r
                        rgbData[pos2 + 1] = color2.g
                        rgbData[pos2 + 2] = color2.b
                    }
                }
                y += 1
            }
        }
        
        return ProcessedFrame(number: frameNumber, rgbData: rgbData, width: frameWidth, height: frameHeight)
    }
}
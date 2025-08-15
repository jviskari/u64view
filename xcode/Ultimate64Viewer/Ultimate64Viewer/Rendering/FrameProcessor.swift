import Foundation

class FrameProcessor {
    private var currentFrameNumber: UInt16 = 0
    private var packetsCollected: [UInt16: Data] = [:] // Use dictionary for out-of-order packets
    private let packetsPerFrame = 68
    private let frameWidth = 384
    private let frameHeight = 272
    
    private var frameStarted = false
    private let lock = NSLock()
    
    // Jitter handling
    private var frameTimeout: Date?
    private let frameTimeoutInterval: TimeInterval = 0.1 // 100ms timeout for incomplete frames
    
    // Frame loss tracking
    private var framesReceived = 0
    private var framesLost = 0
    private var lastStatsReport = Date()
    
    func processPacket(_ data: Data) -> ProcessedFrame? {
        lock.lock()
        defer { lock.unlock() }
        
        guard data.count >= 12 else { return nil }
        
        let header = PacketHeader(from: data)
        
        // Check for frame timeout
        if let timeout = frameTimeout, Date() > timeout {
            // Current frame timed out, abandon it
            if packetsCollected.count > 0 {
                framesLost += 1
                reportStatsIfNeeded()
            }
            packetsCollected.removeAll()
            frameStarted = false
            frameTimeout = nil
        }
        
        // Check if this is a new frame
        if !frameStarted || header.frm != currentFrameNumber {
            // Process the previous frame if we have enough packets
            var completedFrame: ProcessedFrame? = nil
            if frameStarted && packetsCollected.count >= packetsPerFrame * 3/4 { // Accept frame if we have 75% of packets
                completedFrame = assembleFrame(frameNumber: currentFrameNumber, packets: packetsCollected)
                framesReceived += 1
            } else if frameStarted && packetsCollected.count > 0 {
                framesLost += 1
            }
            
            // Start new frame
            currentFrameNumber = header.frm
            packetsCollected.removeAll()
            frameStarted = true
            frameTimeout = Date().addingTimeInterval(frameTimeoutInterval)
            
            // Add this packet to the new frame
            let pixelData = data.dropFirst(12)
            packetsCollected[header.lin] = Data(pixelData)
            
            reportStatsIfNeeded()
            return completedFrame
        }
        
        // Add packet data to current frame (use line number as key for ordering)
        let pixelData = data.dropFirst(12)
        packetsCollected[header.lin] = Data(pixelData)
        
        // Check if frame is complete
        if packetsCollected.count == packetsPerFrame {
            let frame = assembleFrame(frameNumber: currentFrameNumber, packets: packetsCollected)
            packetsCollected.removeAll()
            frameStarted = false
            frameTimeout = nil
            framesReceived += 1
            return frame
        }
        
        return nil
    }
    
    private func assembleFrame(frameNumber: UInt16, packets: [UInt16: Data]) -> ProcessedFrame {
        var rgbData = Data(count: frameWidth * frameHeight * 3)
        
        // Sort packets by line number
        let sortedPackets = packets.sorted { $0.key < $1.key }
        
        var y = 0
        
        for (lineNumber, packetData) in sortedPackets {
            guard y < frameHeight else { break }
            
            var pixelIndex = 0
            
            // Each packet contains 4 lines of data
            for lineInPacket in 0..<4 {
                guard y < frameHeight else { break }
                
                // Each line has 192 bytes (384 pixels / 2 pixels per byte)
                for x in stride(from: 0, to: min(192, frameWidth / 2), by: 1) {
                    guard pixelIndex < packetData.count else { break }
                    
                    let byte = packetData[pixelIndex]
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
    
    private func reportStatsIfNeeded() {
        let now = Date()
        if now.timeIntervalSince(lastStatsReport) > 10.0 { // Report every 10 seconds
            let totalFrames = framesReceived + framesLost
            if totalFrames > 0 {
                let lossRate = Double(framesLost) / Double(totalFrames) * 100.0
                if lossRate > 0.1 { // Only report if loss rate > 0.1%
                    print("📺 Video: \(framesLost)/\(totalFrames) frames lost (\(String(format: "%.2f", lossRate))%)")
                }
            }
            lastStatsReport = now
            framesLost = 0
            framesReceived = 0
        }
    }
}
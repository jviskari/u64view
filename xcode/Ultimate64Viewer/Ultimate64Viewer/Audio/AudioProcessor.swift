import Foundation

class AudioProcessor {
    private var expectedSequenceNumber: UInt16 = 0
    private var hasStarted = false
    private let lock = NSLock()
    
    // Jitter buffer - stores packets temporarily to handle out-of-order delivery
    private var jitterBuffer: [UInt16: Data] = [:]
    private var bufferSize: Int = 5 // Buffer up to 5 packets
    private var playoutSequence: UInt16 = 0
    private var bufferFilled = false
    
    // Packet loss detection
    private var packetsReceived = 0
    private var packetsLost = 0
    private var lastStatsReport = Date()
    
    // Audio format constants
    private let sampleRate = 48000
    private let channels = 2
    private let bitsPerSample = 16
    private let samplesPerPacket = 192
    
    func processPacket(_ data: Data) -> ProcessedAudio? {
        lock.lock()
        defer { lock.unlock() }
        
        guard data.count >= 2 else { return nil }
        
        let header = AudioPacketHeader(from: data)
        let audioData = data.dropFirst(2)
        
        // Initialize on first packet
        if !hasStarted {
            expectedSequenceNumber = header.sequenceNumber
            playoutSequence = header.sequenceNumber
            hasStarted = true
        }
        
        packetsReceived += 1
        
        // Add packet to jitter buffer
        jitterBuffer[header.sequenceNumber] = Data(audioData)
        
        // Remove old packets from buffer (older than buffer size)
        let oldestAllowed = playoutSequence &- UInt16(bufferSize * 2)
        jitterBuffer = jitterBuffer.filter { seq, _ in
            seq >= oldestAllowed
        }
        
        // Check if we should start playing (buffer has some packets)
        if !bufferFilled && jitterBuffer.count >= bufferSize / 2 {
            bufferFilled = true
        }
        
        // Try to get next packet in sequence
        if bufferFilled, let packetData = jitterBuffer.removeValue(forKey: playoutSequence) {
            let processedAudio = ProcessedAudio(
                sequenceNumber: playoutSequence,
                pcmData: packetData,
                sampleRate: sampleRate,
                channels: channels,
                bitsPerSample: bitsPerSample
            )
            
            playoutSequence = playoutSequence &+ 1
            return processedAudio
        }
        
        // Handle missing packets - if we're missing the next packet for too long, skip it
        if bufferFilled && !jitterBuffer.keys.contains(playoutSequence) {
            let availablePackets = jitterBuffer.keys.sorted()
            
            // If we have packets ahead of what we're waiting for, skip the missing one
            if let nextAvailable = availablePackets.first, nextAvailable > playoutSequence {
                // Generate silence for missing packet
                let silenceData = Data(repeating: 0, count: samplesPerPacket * channels * 2) // 16-bit stereo
                let silenceAudio = ProcessedAudio(
                    sequenceNumber: playoutSequence,
                    pcmData: silenceData,
                    sampleRate: sampleRate,
                    channels: channels,
                    bitsPerSample: bitsPerSample
                )
                
                packetsLost += 1
                playoutSequence = playoutSequence &+ 1
                
                // Report stats occasionally
                reportStatsIfNeeded()
                
                return silenceAudio
            }
        }
        
        return nil
    }
    
    private func reportStatsIfNeeded() {
        let now = Date()
        if now.timeIntervalSince(lastStatsReport) > 10.0 { // Report every 10 seconds
            let totalPackets = packetsReceived + packetsLost
            if totalPackets > 0 {
                let lossRate = Double(packetsLost) / Double(totalPackets) * 100.0
                if lossRate > 0.1 { // Only report if loss rate > 0.1%
                    print("🎵 Audio: \(packetsLost)/\(totalPackets) packets lost (\(String(format: "%.2f", lossRate))%)")
                }
            }
            lastStatsReport = now
            packetsLost = 0
            packetsReceived = 0
        }
    }
}
import Foundation

class AudioProcessor {
    private var expectedSequenceNumber: UInt16 = 0
    private var audioBuffer = Data()
    private let lock = NSLock()
    private var hasStarted = false
    
    // Audio format constants
    private let sampleRate = 48000
    private let channels = 2
    private let bitsPerSample = 16
    
    func processPacket(_ data: Data) -> ProcessedAudio? {
        lock.lock()
        defer { lock.unlock() }
        
        guard data.count >= 2 else { 
            return nil 
        }
        
        let header = AudioPacketHeader(from: data)
        
        // Initialize sequence tracking on first packet
        if !hasStarted {
            expectedSequenceNumber = header.sequenceNumber
            hasStarted = true
        }
        
        // Check for dropped packets
        if header.sequenceNumber != expectedSequenceNumber {
            expectedSequenceNumber = header.sequenceNumber
        }
        
        // Extract audio data (skip 2-byte header)
        let audioData = data.dropFirst(2)
        audioBuffer.append(audioData)
        
        // Update expected sequence number for next packet
        expectedSequenceNumber = header.sequenceNumber &+ 1
        
        // Return processed audio with current buffer
        let processedAudio = ProcessedAudio(
            sequenceNumber: header.sequenceNumber,
            pcmData: Data(audioData),
            sampleRate: sampleRate,
            channels: channels,
            bitsPerSample: bitsPerSample
        )
        
        return processedAudio
    }
}
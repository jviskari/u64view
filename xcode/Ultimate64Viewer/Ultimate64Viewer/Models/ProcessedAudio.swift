import Foundation

struct ProcessedAudio {
    let sequenceNumber: UInt16
    let pcmData: Data
    let sampleRate: Int
    let channels: Int
    let bitsPerSample: Int
    
    init(sequenceNumber: UInt16, pcmData: Data, sampleRate: Int = 48000, channels: Int = 2, bitsPerSample: Int = 16) {
        self.sequenceNumber = sequenceNumber
        self.pcmData = pcmData
        self.sampleRate = sampleRate
        self.channels = channels
        self.bitsPerSample = bitsPerSample
    }
}
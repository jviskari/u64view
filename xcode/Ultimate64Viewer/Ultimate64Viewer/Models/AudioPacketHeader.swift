import Foundation

struct AudioPacketHeader {
    let sequenceNumber: UInt16
    
    init(from data: Data) {
        // Audio packets have 2-byte little-endian sequence number
        self.sequenceNumber = data.withUnsafeBytes { bytes in
            bytes.load(fromByteOffset: 0, as: UInt16.self).littleEndian
        }
    }
}
import Foundation

struct PacketHeader {
    let seq: UInt16
    let frm: UInt16
    let lin: UInt16
    let width: UInt16
    let lp: UInt8
    let bp: UInt8
    let enc: UInt16
    
    init(from data: Data) {
        guard data.count >= 12 else {
            self.seq = 0
            self.frm = 0
            self.lin = 0
            self.width = 0
            self.lp = 0
            self.bp = 0
            self.enc = 0
            return
        }
        
        // Parse little-endian data
        self.seq = data.withUnsafeBytes { $0.load(fromByteOffset: 0, as: UInt16.self) }
        self.frm = data.withUnsafeBytes { $0.load(fromByteOffset: 2, as: UInt16.self) }
        self.lin = data.withUnsafeBytes { $0.load(fromByteOffset: 4, as: UInt16.self) }
        self.width = data.withUnsafeBytes { $0.load(fromByteOffset: 6, as: UInt16.self) }
        self.lp = data.withUnsafeBytes { $0.load(fromByteOffset: 8, as: UInt8.self) }
        self.bp = data.withUnsafeBytes { $0.load(fromByteOffset: 9, as: UInt8.self) }
        self.enc = data.withUnsafeBytes { $0.load(fromByteOffset: 10, as: UInt16.self) }
    }
}
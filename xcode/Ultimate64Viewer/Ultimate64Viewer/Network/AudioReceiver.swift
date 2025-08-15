import Foundation

#if canImport(Darwin)
import Darwin
#else
import Glibc
#endif

protocol AudioReceiverDelegate: AnyObject {
    func audioReceiver(_ receiver: AudioReceiver, didReceivePacket data: Data)
    func audioReceiver(_ receiver: AudioReceiver, didEncounterError error: Error)
}

class AudioReceiver {
    weak var delegate: AudioReceiverDelegate?
    
    private let multicastGroup: String
    private let port: UInt16
    private var socketFD: Int32 = -1
    private let queue = DispatchQueue(label: "audio.receiver", qos: .userInitiated)
    private var source: DispatchSourceRead?
    
    private var isReceiving = false
    
    init(multicastGroup: String = "239.0.1.64", port: UInt16 = 11001) {
        self.multicastGroup = multicastGroup
        self.port = port
    }
    
    func startReceiving() {
        guard !isReceiving else { return }
        
        print("🎵 AudioReceiver: Starting to receive on \(multicastGroup):\(port)")
        
        queue.async { [weak self] in
            do {
                try self?.setupSocket()
                try self?.joinMulticastGroup()
                self?.startListening()
                self?.isReceiving = true
                print("🎵 AudioReceiver: Successfully started receiving")
            } catch {
                print("🎵 AudioReceiver: Failed to start - \(error)")
                self?.cleanup()
                DispatchQueue.main.async {
                    self?.delegate?.audioReceiver(self!, didEncounterError: error)
                }
            }
        }
    }
    
    private func setupSocket() throws {
        socketFD = socket(AF_INET, SOCK_DGRAM, 0)
        guard socketFD >= 0 else {
            throw NetworkError.socketCreationFailed
        }
        
        let flags = fcntl(socketFD, F_GETFL, 0)
        guard flags != -1 else {
            throw NetworkError.socketOptionFailed
        }
        guard fcntl(socketFD, F_SETFL, flags | O_NONBLOCK) != -1 else {
            throw NetworkError.socketOptionFailed
        }
        
        var reuseAddr: Int32 = 1
        guard setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, socklen_t(MemoryLayout<Int32>.size)) == 0 else {
            throw NetworkError.socketOptionFailed
        }
        
        var serverAddr = sockaddr_in()
        serverAddr.sin_family = sa_family_t(AF_INET)
        serverAddr.sin_addr.s_addr = INADDR_ANY
        serverAddr.sin_port = port.bigEndian
        
        let bindResult = withUnsafePointer(to: &serverAddr) {
            $0.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                bind(socketFD, $0, socklen_t(MemoryLayout<sockaddr_in>.size))
            }
        }
        
        guard bindResult == 0 else {
            throw NetworkError.bindFailed
        }
    }
    
    private func joinMulticastGroup() throws {
        var mreq = ip_mreq()
        mreq.imr_multiaddr.s_addr = inet_addr(multicastGroup)
        mreq.imr_interface.s_addr = INADDR_ANY
        
        guard setsockopt(socketFD, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, socklen_t(MemoryLayout<ip_mreq>.size)) == 0 else {
            throw NetworkError.multicastJoinFailed
        }
    }
    
    private func startListening() {
        source = DispatchSource.makeReadSource(fileDescriptor: socketFD, queue: queue)
        
        source?.setEventHandler { [weak self] in
            self?.handleSocketData()
        }
        
        source?.setCancelHandler { [weak self] in
            if let fd = self?.socketFD, fd >= 0 {
                close(fd)
                self?.socketFD = -1
            }
        }
        
        source?.resume()
    }
    
    private func handleSocketData() {
        var buffer = [UInt8](repeating: 0, count: 1024)
        
        let bytesReceived = recv(socketFD, &buffer, buffer.count, 0)
        
        if bytesReceived > 0 {
            let data = Data(buffer.prefix(bytesReceived))
            print("🎵 AudioReceiver: Received \(bytesReceived) bytes")
            
            DispatchQueue.main.async {
                self.delegate?.audioReceiver(self, didReceivePacket: data)
            }
        } else if bytesReceived == -1 {
            let error = errno
            if error != EAGAIN && error != EWOULDBLOCK {
                print("🎵 AudioReceiver: Receive error - \(error)")
                DispatchQueue.main.async {
                    self.delegate?.audioReceiver(self, didEncounterError: NetworkError.receiveError)
                }
            }
        }
    }
    
    func stopReceiving() {
        guard isReceiving else { return }
        
        print("🎵 AudioReceiver: Stopping")
        isReceiving = false
        source?.cancel()
        source = nil
        cleanup()
    }
    
    private func cleanup() {
        if socketFD >= 0 {
            close(socketFD)
            socketFD = -1
        }
    }
    
    deinit {
        stopReceiving()
    }
}
import Foundation

#if canImport(Darwin)
import Darwin
#else
import Glibc
#endif

protocol NetworkReceiverDelegate: AnyObject {
    func networkReceiver(_ receiver: NetworkReceiver, didReceivePacket data: Data)
    func networkReceiver(_ receiver: NetworkReceiver, didEncounterError error: Error)
}

class NetworkReceiver {
    weak var delegate: NetworkReceiverDelegate?
    
    private let multicastGroup: String
    private let port: UInt16
    private var socketFD: Int32 = -1
    private let queue = DispatchQueue(label: "network.receiver", qos: .userInitiated)
    private var source: DispatchSourceRead?
    
    private var isReceiving = false
    
    init(multicastGroup: String = "239.0.1.64", port: UInt16 = 11000) {
        self.multicastGroup = multicastGroup
        self.port = port
    }
    
    func startReceiving() {
        guard !isReceiving else { return }
        
        queue.async { [weak self] in
            do {
                try self?.setupSocket()
                try self?.joinMulticastGroup()
                self?.startListening()
                self?.isReceiving = true
            } catch {
                self?.cleanup()
                DispatchQueue.main.async {
                    self?.delegate?.networkReceiver(self!, didEncounterError: error)
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
            
            DispatchQueue.main.async {
                self.delegate?.networkReceiver(self, didReceivePacket: data)
            }
        } else if bytesReceived == -1 {
            let error = errno
            if error != EAGAIN && error != EWOULDBLOCK {
                // Only report actual errors, not "would block" conditions
                DispatchQueue.main.async {
                    self.delegate?.networkReceiver(self, didEncounterError: NetworkError.receiveError)
                }
            }
        }
    }
    
    func stopReceiving() {
        guard isReceiving else { return }
        
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

enum NetworkError: Error, LocalizedError {
    case socketCreationFailed
    case socketOptionFailed
    case bindFailed
    case invalidMulticastAddress
    case multicastJoinFailed
    case receiveError
    
    var errorDescription: String? {
        switch self {
        case .socketCreationFailed:
            return "Failed to create socket"
        case .socketOptionFailed:
            return "Failed to set socket options"
        case .bindFailed:
            return "Failed to bind socket to port"
        case .invalidMulticastAddress:
            return "Invalid multicast address"
        case .multicastJoinFailed:
            return "Failed to join multicast group"
        case .receiveError:
            return "Network receive error"
        }
    }
}
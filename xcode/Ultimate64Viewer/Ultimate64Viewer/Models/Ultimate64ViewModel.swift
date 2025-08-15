import Foundation
import Combine

@MainActor
class Ultimate64ViewModel: ObservableObject {
    @Published var currentFrame: ProcessedFrame?
    @Published var sourceIP: String?
    @Published var showConnectionInfo = false
    @Published var isConnected = false // Add connection state
    
    nonisolated private let networkReceiver = NetworkReceiver()
    private let frameProcessor = FrameProcessor()
    private var frameQueue: [ProcessedFrame] = []
    private var displayTimer: Timer?
    private var connectionInfoTimer: Timer?
    private var timeoutTimer: Timer? // Add timeout timer
    
    private let frameInterval: TimeInterval = 1.0 / 50.0 // PAL 50Hz
    private let maxQueueSize = 3
    private let connectionTimeout: TimeInterval = 5.0 // 5 seconds timeout
    
    init() {
        networkReceiver.delegate = self
        startDisplayTimer()
    }
    
    func startReceiving() {
        networkReceiver.startReceiving()
        startTimeoutTimer() // Start monitoring for timeout
    }
    
    func stopReceiving() {
        cleanup()
    }
    
    nonisolated private func cleanup() {
        networkReceiver.stopReceiving()
        Task { @MainActor in
            self.displayTimer?.invalidate()
            self.displayTimer = nil
            self.connectionInfoTimer?.invalidate()
            self.connectionInfoTimer = nil
            self.timeoutTimer?.invalidate()
            self.timeoutTimer = nil
            self.frameQueue.removeAll()
        }
    }
    
    private func startDisplayTimer() {
        displayTimer = Timer.scheduledTimer(withTimeInterval: frameInterval, repeats: true) { [weak self] _ in
            Task { @MainActor in
                self?.displayNextFrame()
            }
        }
    }
    
    private func startTimeoutTimer() {
        timeoutTimer?.invalidate()
        timeoutTimer = Timer.scheduledTimer(withTimeInterval: connectionTimeout, repeats: false) { [weak self] _ in
            Task { @MainActor in
                self?.handleConnectionTimeout()
            }
        }
    }
    
    private func resetTimeoutTimer() {
        startTimeoutTimer() // Restart the timeout timer
    }
    
    private func handleConnectionTimeout() {
        // Connection lost - fall back to waiting state
        isConnected = false
        currentFrame = nil
        sourceIP = nil
        showConnectionInfo = false
        frameQueue.removeAll()
        
        // Keep monitoring for new connections
        startTimeoutTimer()
    }
    
    private func displayNextFrame() {
        guard !frameQueue.isEmpty else { return }
        
        // Always show the latest frame to minimize latency
        currentFrame = frameQueue.removeLast()
        frameQueue.removeAll()
        
        // Mark as connected and show connection info briefly when first receiving
        if !isConnected {
            isConnected = true
            showConnectionInfo = true
            connectionInfoTimer = Timer.scheduledTimer(withTimeInterval: 3.0, repeats: false) { [weak self] _ in
                Task { @MainActor in
                    self?.showConnectionInfo = false
                }
            }
        }
    }
    
    private func addFrameToQueue(_ frame: ProcessedFrame) {
        // Keep only the latest frames
        while frameQueue.count >= maxQueueSize {
            frameQueue.removeFirst()
        }
        frameQueue.append(frame)
    }
    
    deinit {
        cleanup()
    }
}

extension Ultimate64ViewModel: NetworkReceiverDelegate {
    nonisolated func networkReceiver(_ receiver: NetworkReceiver, didReceivePacket data: Data) {
        Task { @MainActor in
            // Reset timeout timer - we received a packet
            self.resetTimeoutTimer()
            
            // Get source IP from the receiver
            if let source = receiver.lastSourceIP, self.sourceIP != source {
                self.sourceIP = source
            }
            
            if let frame = self.frameProcessor.processPacket(data) {
                self.addFrameToQueue(frame)
            }
        }
    }
    
    nonisolated func networkReceiver(_ receiver: NetworkReceiver, didEncounterError error: Error) {
        // Silently handle errors
    }
}
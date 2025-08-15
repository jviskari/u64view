import Foundation
import Combine

@MainActor
class Ultimate64ViewModel: ObservableObject {
    @Published var currentFrame: ProcessedFrame?
    @Published var sourceIP: String?
    @Published var showConnectionInfo = false
    @Published var isConnected = false
    
    nonisolated private let networkReceiver = NetworkReceiver()
    nonisolated private let audioReceiver = AudioReceiver()
    private let frameProcessor = FrameProcessor()
    private let audioProcessor = AudioProcessor()
    private let audioPlayer = AudioPlayer()
    
    private var frameQueue: [ProcessedFrame] = []
    private var displayTimer: Timer?
    private var connectionInfoTimer: Timer?
    private var timeoutTimer: Timer?
    
    private let frameInterval: TimeInterval = 1.0 / 50.0
    private let maxQueueSize = 3
    private let connectionTimeout: TimeInterval = 5.0
    
    private var isShuttingDown = false
    
    init() {
        networkReceiver.delegate = self
        audioReceiver.delegate = self
        startDisplayTimer()
        
        // Auto-start both video and audio
        startReceiving()
        audioReceiver.startReceiving()
        audioPlayer.startPlayback()
    }
    
    func startReceiving() {
        guard !isShuttingDown else { return }
        networkReceiver.startReceiving()
        startTimeoutTimer()
    }
    
    func stopReceiving() {
        isShuttingDown = true
        cleanup()
    }
    
    nonisolated private func cleanup() {
        networkReceiver.stopReceiving()
        audioReceiver.stopReceiving()
        
        Task { @MainActor in
            self.stopAllTimers()
            self.frameQueue.removeAll()
            self.currentFrame = nil
            self.sourceIP = nil
            self.isConnected = false
            self.showConnectionInfo = false
        }
    }
    
    private func stopAllTimers() {
        displayTimer?.invalidate()
        displayTimer = nil
        
        connectionInfoTimer?.invalidate()
        connectionInfoTimer = nil
        
        timeoutTimer?.invalidate()
        timeoutTimer = nil
    }
    
    private func startDisplayTimer() {
        guard !isShuttingDown else { return }
        displayTimer = Timer.scheduledTimer(withTimeInterval: frameInterval, repeats: true) { [weak self] _ in
            Task { @MainActor in
                guard let self = self, !self.isShuttingDown else { return }
                self.displayNextFrame()
            }
        }
    }
    
    private func startTimeoutTimer() {
        guard !isShuttingDown else { return }
        timeoutTimer?.invalidate()
        timeoutTimer = Timer.scheduledTimer(withTimeInterval: connectionTimeout, repeats: false) { [weak self] _ in
            Task { @MainActor in
                guard let self = self, !self.isShuttingDown else { return }
                self.handleConnectionTimeout()
            }
        }
    }
    
    private func resetTimeoutTimer() {
        guard !isShuttingDown else { return }
        startTimeoutTimer()
    }
    
    private func handleConnectionTimeout() {
        guard !isShuttingDown else { return }
        isConnected = false
        currentFrame = nil
        showConnectionInfo = false
        frameQueue.removeAll()
        startTimeoutTimer()
    }
    
    private func displayNextFrame() {
        guard !frameQueue.isEmpty && !isShuttingDown else { return }
        
        // Display the most recent frame (drop old ones for low latency)
        currentFrame = frameQueue.removeLast()
        frameQueue.removeAll()
        
        if !isConnected {
            isConnected = true
            showConnectionInfo = true
            connectionInfoTimer = Timer.scheduledTimer(withTimeInterval: 3.0, repeats: false) { [weak self] _ in
                Task { @MainActor in
                    guard let self = self, !self.isShuttingDown else { return }
                    self.showConnectionInfo = false
                }
            }
        }
    }
    
    private func addFrameToQueue(_ frame: ProcessedFrame) {
        guard !isShuttingDown else { return }
        
        while frameQueue.count >= maxQueueSize {
            frameQueue.removeFirst()
        }
        frameQueue.append(frame)
    }
    
    deinit {
        isShuttingDown = true
        cleanup()
    }
}

extension Ultimate64ViewModel: NetworkReceiverDelegate {
    nonisolated func networkReceiver(_ receiver: NetworkReceiver, didReceivePacket data: Data) {
        Task { @MainActor in
            guard !self.isShuttingDown else { return }
            
            self.resetTimeoutTimer()
            
            if let source = receiver.lastSourceIP, self.sourceIP != source {
                self.sourceIP = source
            }
            
            if let frame = self.frameProcessor.processPacket(data) {
                self.addFrameToQueue(frame)
            }
        }
    }
    
    nonisolated func networkReceiver(_ receiver: NetworkReceiver, didEncounterError error: Error) {
        // Silently handle errors during normal operation
    }
}

extension Ultimate64ViewModel: AudioReceiverDelegate {
    nonisolated func audioReceiver(_ receiver: AudioReceiver, didReceivePacket data: Data) {
        Task { @MainActor in
            guard !self.isShuttingDown else { return }
            
            // Process and play audio immediately - no queuing or synchronization delays
            if let processedAudio = self.audioProcessor.processPacket(data) {
                self.audioPlayer.playAudioData(processedAudio.pcmData)
            }
        }
    }
    
    nonisolated func audioReceiver(_ receiver: AudioReceiver, didEncounterError error: Error) {
        // Silently handle audio errors
    }
}
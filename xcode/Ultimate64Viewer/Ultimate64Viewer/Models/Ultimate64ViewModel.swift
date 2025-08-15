import Foundation
import Combine

@MainActor
class Ultimate64ViewModel: ObservableObject {
    @Published var currentFrame: ProcessedFrame?
    
    nonisolated private let networkReceiver = NetworkReceiver()
    private let frameProcessor = FrameProcessor()
    private var frameQueue: [ProcessedFrame] = []
    private var displayTimer: Timer?
    
    private let frameInterval: TimeInterval = 1.0 / 50.0 // PAL 50Hz
    private let maxQueueSize = 3
    
    init() {
        networkReceiver.delegate = self
        startDisplayTimer()
    }
    
    func startReceiving() {
        networkReceiver.startReceiving()
    }
    
    func stopReceiving() {
        cleanup()
    }
    
    nonisolated private func cleanup() {
        networkReceiver.stopReceiving()
        Task { @MainActor in
            self.displayTimer?.invalidate()
            self.displayTimer = nil
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
    
    private func displayNextFrame() {
        guard !frameQueue.isEmpty else { return }
        
        // Always show the latest frame to minimize latency
        currentFrame = frameQueue.removeLast()
        frameQueue.removeAll()
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
            if let frame = self.frameProcessor.processPacket(data) {
                self.addFrameToQueue(frame)
            }
        }
    }
    
    nonisolated func networkReceiver(_ receiver: NetworkReceiver, didEncounterError error: Error) {
        // Silently handle errors - could add logging if needed
    }
}
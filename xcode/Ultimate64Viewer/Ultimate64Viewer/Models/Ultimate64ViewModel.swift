import Foundation
import Combine

@MainActor
class Ultimate64ViewModel: ObservableObject {
    @Published var currentFrame: ProcessedFrame?
    @Published var frameNumber: UInt16 = 0
    @Published var fps: Double = 0.0
    @Published var isReceiving = false
    @Published var errorMessage: String?
    
    // Make networkReceiver nonisolated to avoid actor isolation issues
    nonisolated private let networkReceiver = NetworkReceiver()
    private let frameProcessor = FrameProcessor()
    
    private var frameQueue: [ProcessedFrame] = []
    private let jitterBufferSize = 1
    private let maxQueueSize = 3
    
    private var lastDisplayedFrameNumber: UInt16 = 0
    private var hasLastFrame = false
    
    // FPS calculation
    private var frameTimestamps: [Date] = []
    private let fpsCalculationWindow: TimeInterval = 1.0
    
    // Frame timing
    private let frameInterval: TimeInterval = 1.0 / 60.0
    private var displayTimer: Timer?
    
    init() {
        networkReceiver.delegate = self
        startDisplayTimer()
    }
    
    func startReceiving() {
        guard !isReceiving else { return }
        
        networkReceiver.startReceiving()
        isReceiving = true
        errorMessage = nil
    }
    
    func stopReceiving() {
        guard isReceiving else { return }
        
        cleanup()
        isReceiving = false
    }
    
    // Now cleanup can access networkReceiver since it's nonisolated
    nonisolated private func cleanup() {
        networkReceiver.stopReceiving()
        
        // Clean up MainActor properties on main actor
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
        
        // Skip frames if queue is getting full to reduce latency
        if frameQueue.count > jitterBufferSize {
            let latestFrame = frameQueue.last!
            frameQueue.removeAll()
            frameQueue.append(latestFrame)
        }
        
        let frame = frameQueue.removeFirst()
        
        currentFrame = frame
        frameNumber = frame.number
        lastDisplayedFrameNumber = frame.number
        hasLastFrame = true
        
        updateFPS()
    }
    
    private func addFrameToQueue(_ frame: ProcessedFrame) {
        while frameQueue.count >= maxQueueSize {
            frameQueue.removeFirst()
        }
        
        frameQueue.append(frame)
    }
    
    private func updateFPS() {
        let now = Date()
        frameTimestamps.append(now)
        
        frameTimestamps.removeAll { now.timeIntervalSince($0) > fpsCalculationWindow }
        
        if frameTimestamps.count > 1 {
            fps = Double(frameTimestamps.count - 1) / fpsCalculationWindow
        }
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
        Task { @MainActor in
            self.errorMessage = "Network error: \(error.localizedDescription)"
        }
    }
}
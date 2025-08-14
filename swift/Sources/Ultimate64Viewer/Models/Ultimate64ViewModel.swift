import Foundation
import Combine

@MainActor
class Ultimate64ViewModel: ObservableObject {
    @Published var currentFrame: ProcessedFrame?
    @Published var frameNumber: UInt16 = 0
    @Published var fps: Double = 0.0
    @Published var isReceiving = false
    @Published var errorMessage: String?
    
    private let networkReceiver = NetworkReceiver()
    private let frameProcessor = FrameProcessor()
    
    private var frameQueue: [ProcessedFrame] = []
    private let jitterBufferSize = 1  // Reduced from 2 for lower latency
    private let maxQueueSize = 3      // Reduced from 10
    
    private var lastDisplayedFrameNumber: UInt16 = 0
    private var hasLastFrame = false
    
    // FPS calculation
    private var frameTimestamps: [Date] = []
    private let fpsCalculationWindow: TimeInterval = 1.0  // Reduced window
    
    // Frame timing - more aggressive
    private let frameInterval: TimeInterval = 1.0 / 60.0 // 60Hz display refresh
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
        
        print("Started receiving Ultimate 64 video stream...")
    }
    
    func stopReceiving() {
        guard isReceiving else { return }
        
        networkReceiver.stopReceiving()
        isReceiving = false
        
        print("Stopped receiving video stream")
    }
    
    private func startDisplayTimer() {
        displayTimer = Timer.scheduledTimer(withTimeInterval: frameInterval, repeats: true) { [weak self] _ in
            Task { @MainActor in
                self?.displayNextFrame()
            }
        }
    }
    
    private func displayNextFrame() {
        // More aggressive frame display - don't wait for jitter buffer if we have any frames
        guard !frameQueue.isEmpty else { return }
        
        // If we have too many frames, skip to the latest to reduce latency
        if frameQueue.count > jitterBufferSize {
            print("Skipping \(frameQueue.count - 1) frames to reduce latency")
            // Keep only the latest frame
            let latestFrame = frameQueue.last!
            frameQueue.removeAll()
            frameQueue.append(latestFrame)
        }
        
        let frame = frameQueue.removeFirst()
        
        // Always display the frame - don't check sequence numbers for now
        currentFrame = frame
        frameNumber = frame.number
        lastDisplayedFrameNumber = frame.number
        hasLastFrame = true
        
        updateFPS()
    }
    
    private func addFrameToQueue(_ frame: ProcessedFrame) {
        // Remove debug output for performance
        
        // Keep queue small for low latency
        while frameQueue.count >= maxQueueSize {
            frameQueue.removeFirst()
        }
        
        frameQueue.append(frame)
    }
    
    private func updateFPS() {
        let now = Date()
        frameTimestamps.append(now)
        
        // Remove old timestamps outside the calculation window
        frameTimestamps.removeAll { now.timeIntervalSince($0) > fpsCalculationWindow }
        
        // Calculate FPS
        if frameTimestamps.count > 1 {
            fps = Double(frameTimestamps.count - 1) / fpsCalculationWindow
        }
    }
    
    deinit {
        displayTimer?.invalidate()
    }
}

extension Ultimate64ViewModel: NetworkReceiverDelegate {
    nonisolated func networkReceiver(_ receiver: NetworkReceiver, didReceivePacket data: Data) {
        Task { @MainActor in
            if let frame = self.frameProcessor.processPacket(data) {
                // Remove debug output for performance
                self.addFrameToQueue(frame)
            }
        }
    }
    
    nonisolated func networkReceiver(_ receiver: NetworkReceiver, didEncounterError error: Error) {
        Task { @MainActor in
            self.errorMessage = "Network error: \(error.localizedDescription)"
            print("Network error: \(error)")
        }
    }
}
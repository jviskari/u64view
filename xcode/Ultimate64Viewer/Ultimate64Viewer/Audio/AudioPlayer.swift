import Foundation
import AVFoundation

#if os(iOS)
import UIKit
#elseif os(macOS)
import AppKit
#endif

class AudioPlayer {
    private let audioEngine = AVAudioEngine()
    private let playerNode = AVAudioPlayerNode()
    private let audioFormat: AVAudioFormat
    
    private var isPlaying = false
    private var isEngineSetup = false
    private let audioQueue = DispatchQueue(label: "audio.playback", qos: .userInteractive)
    
    init() {
        print("🎵 AudioPlayer: Initializing...")
        
        // Use the output format from the main mixer node for compatibility
        let outputFormat = AVAudioEngine().mainMixerNode.outputFormat(forBus: 0)
        print("🎵 AudioPlayer: System output format: \(outputFormat)")
        
        // Create a compatible format - use system sample rate but our desired format
        guard let format = AVAudioFormat(commonFormat: .pcmFormatFloat32,
                                       sampleRate: outputFormat.sampleRate,
                                       channels: 2,
                                       interleaved: false) else {
            fatalError("Could not create audio format")
        }
        self.audioFormat = format
        
        print("🎵 AudioPlayer: Using audio format: \(format)")
        
        // Setup audio session but don't start the engine yet
        setupAudioSession()
        print("🎵 AudioPlayer: Initialization complete")
    }
    
    private func setupAudioSession() {
        #if os(iOS)
        // iOS audio session setup
        do {
            let audioSession = AVAudioSession.sharedInstance()
            try audioSession.setCategory(.playback, mode: .default, options: [])
            try audioSession.setActive(true)
            print("🎵 AudioPlayer: iOS audio session configured successfully")
        } catch {
            print("🎵 AudioPlayer: Failed to setup iOS audio session: \(error)")
        }
        #elseif os(macOS)
        // macOS doesn't need audio session configuration
        print("🎵 AudioPlayer: macOS - no audio session setup needed")
        #endif
    }
    
    private func setupAudioEngine() {
        guard !isEngineSetup else { return }
        
        print("🎵 AudioPlayer: Setting up audio engine...")
        
        do {
            // Attach player node
            audioEngine.attach(playerNode)
            
            // Connect with explicit format
            audioEngine.connect(playerNode, to: audioEngine.mainMixerNode, format: audioFormat)
            
            // Set output volume
            audioEngine.mainMixerNode.outputVolume = 1.0
            
            // Prepare the engine
            audioEngine.prepare()
            
            isEngineSetup = true
            print("🎵 AudioPlayer: Audio engine setup complete")
            
        } catch {
            print("🎵 AudioPlayer: Failed to setup audio engine: \(error)")
            isEngineSetup = false
        }
    }
    
    func startPlayback() {
        guard !isPlaying else { return }
        
        print("🎵 AudioPlayer: Starting playback...")
        
        audioQueue.async { [weak self] in
            guard let self = self else { return }
            
            // Setup engine if not already done
            if !self.isEngineSetup {
                self.setupAudioEngine()
            }
            
            guard self.isEngineSetup else {
                print("🎵 AudioPlayer: Cannot start - engine setup failed")
                return
            }
            
            #if os(iOS)
            // Reactivate audio session on iOS if needed
            do {
                try AVAudioSession.sharedInstance().setActive(true)
            } catch {
                print("🎵 AudioPlayer: Failed to reactivate iOS audio session: \(error)")
            }
            #endif
            
            // Start the audio engine
            if !self.audioEngine.isRunning {
                do {
                    try self.audioEngine.start()
                    print("🎵 AudioPlayer: Audio engine started")
                } catch {
                    print("🎵 AudioPlayer: Failed to start audio engine: \(error)")
                    return
                }
            }
            
            // Start the player node
            self.playerNode.play()
            self.isPlaying = true
            print("🎵 AudioPlayer: Player node started playing")
        }
    }
    
    func stopPlayback() {
        guard isPlaying else { return }
        
        print("🎵 AudioPlayer: Stopping playback...")
        
        audioQueue.async { [weak self] in
            guard let self = self else { return }
            
            self.playerNode.stop()
            self.isPlaying = false
            print("🎵 AudioPlayer: Player node stopped")
            
            // Stop the audio engine
            if self.audioEngine.isRunning {
                self.audioEngine.stop()
                print("🎵 AudioPlayer: Audio engine stopped")
            }
            
            #if os(iOS)
            // Optionally deactivate audio session on iOS
            do {
                try AVAudioSession.sharedInstance().setActive(false, options: .notifyOthersOnDeactivation)
            } catch {
                print("🎵 AudioPlayer: Failed to deactivate iOS audio session: \(error)")
            }
            #endif
        }
    }
    
    func playAudioData(_ pcmData: Data) {
        guard isPlaying else { 
            return 
        }
        
        guard !pcmData.isEmpty else {
            return
        }
        
        audioQueue.async { [weak self] in
            guard let self = self else { return }
            
            // Convert Int16 PCM data to Float32 for compatibility
            let int16Data = pcmData.withUnsafeBytes { bytes in
                Array(bytes.bindMemory(to: Int16.self))
            }
            
            let frameCount = int16Data.count / 2 // Stereo = 2 channels
            guard frameCount > 0 else {
                return
            }
            
            guard let audioBuffer = AVAudioPCMBuffer(pcmFormat: self.audioFormat, frameCapacity: AVAudioFrameCount(frameCount)) else {
                print("🎵 AudioPlayer: Failed to create audio buffer")
                return
            }
            
            audioBuffer.frameLength = AVAudioFrameCount(frameCount)
            
            // Convert Int16 to Float32 and deinterleave
            guard let leftChannel = audioBuffer.floatChannelData?[0],
                  let rightChannel = audioBuffer.floatChannelData?[1] else {
                print("🎵 AudioPlayer: Failed to get channel data")
                return
            }
            
            for i in 0..<frameCount {
                let leftSample = Float(int16Data[i * 2]) / Float(Int16.max)
                let rightSample = Float(int16Data[i * 2 + 1]) / Float(Int16.max)
                
                leftChannel[i] = leftSample
                rightChannel[i] = rightSample
            }
            
            // Schedule buffer for playback
            self.playerNode.scheduleBuffer(audioBuffer, completionHandler: nil)
        }
    }
    
    var isCurrentlyPlaying: Bool {
        return isPlaying && playerNode.isPlaying
    }
    
    deinit {
        stopPlayback()
        if audioEngine.isRunning {
            audioEngine.stop()
        }
    }
}
#ifndef CAPTURE_INTERFACE_H
#define CAPTURE_INTERFACE_H

#include "video_stream_receiver.h"
#include "audio_stream_receiver.h"
#include <string>
#include <chrono>

// Capture statistics
struct CaptureStats {
    size_t video_frames_captured;
    size_t video_frames_dropped;
    size_t audio_packets_captured;
    size_t audio_packets_dropped;
    std::chrono::high_resolution_clock::time_point start_time;
    std::chrono::milliseconds duration;
    size_t bytes_written;
};

// Capture configuration
struct CaptureConfig {
    std::string output_path;
    bool capture_video = true;
    bool capture_audio = true;
    size_t max_video_queue_size = 100;  // Frames
    size_t max_audio_queue_size = 500;  // Packets
    bool sync_av = true;
    
    // Quality vs latency trade-off
    enum class Mode {
        LIVE,      // Low latency, some drops OK
        QUALITY    // High quality, larger buffers
    } mode = Mode::QUALITY;
};

// Abstract base class for all capture implementations
class CaptureInterface {
public:
    virtual ~CaptureInterface() = default;
    
    // Configuration
    virtual bool initialize(const CaptureConfig& config) = 0;
    
    // Control
    virtual bool startCapture() = 0;
    virtual void stopCapture() = 0;
    virtual bool isCapturing() const = 0;
    
    // Input - these should be thread-safe
    virtual void addVideoFrame(const VideoFrame& frame) = 0;
    virtual void addAudioPacket(const AudioPacket& packet) = 0;
    
    // Status
    virtual CaptureStats getStats() const = 0;
    virtual std::string getStatusString() const = 0;
    
protected:
    CaptureConfig config_;
    CaptureStats stats_;
    std::atomic<bool> capturing_{false};
};

#endif // CAPTURE_INTERFACE_H
#ifndef RAW_CAPTURE_H
#define RAW_CAPTURE_H

#include "capture_interface.h"
#include <fstream>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

// RAW file format structures
struct RawFileHeader {
    char magic[8] = {'U', '6', '4', 'R', 'A', 'W', '1', '0'}; // Version 1.0
    uint32_t video_width;
    uint32_t video_height;
    uint32_t video_fps_num;
    uint32_t video_fps_den;
    uint32_t audio_sample_rate;
    uint16_t audio_channels;
    uint16_t audio_bits_per_sample;
    uint64_t creation_timestamp;
    uint32_t flags; // Future use
    char reserved[32];
} __attribute__((packed));

struct RawFrameHeader {
    char type; // 'V' = video, 'A' = audio
    uint64_t timestamp_us; // Microseconds since capture start
    uint32_t data_size;
    uint16_t frame_number; // For video frames
    uint16_t sequence;     // For audio packets
} __attribute__((packed));

class RawCapture : public CaptureInterface {
private:
    // File output
    std::ofstream output_file_;
    std::string temp_filename_;
    std::string final_filename_;
    
    // Processing queues
    struct TimestampedVideoFrame {
        VideoFrame frame;
        std::chrono::high_resolution_clock::time_point timestamp;
    };
    
    struct TimestampedAudioPacket {
        AudioPacket packet;
        std::chrono::high_resolution_clock::time_point timestamp;
    };
    
    std::queue<TimestampedVideoFrame> video_queue_;
    std::queue<TimestampedAudioPacket> audio_queue_;
    std::mutex video_mutex_;
    std::mutex audio_mutex_;
    std::condition_variable data_available_;
    
    // Processing thread
    std::thread processing_thread_;
    std::atomic<bool> should_stop_{false};
    
    // Timing
    std::chrono::high_resolution_clock::time_point capture_start_time_;
    
    // Synchronization
    bool sync_enabled_;
    std::chrono::microseconds max_av_drift_{40000}; // 40ms max drift
    
public:
    RawCapture();
    ~RawCapture() override;
    
    // CaptureInterface implementation
    bool initialize(const CaptureConfig& config) override;
    bool startCapture() override;
    void stopCapture() override;
    bool isCapturing() const override { return capturing_.load(); }
    
    void addVideoFrame(const VideoFrame& frame) override;
    void addAudioPacket(const AudioPacket& packet) override;
    
    CaptureStats getStats() const override;
    std::string getStatusString() const override;
    
    // RAW-specific methods
    static bool validateRawFile(const std::string& filename);
    static RawFileHeader readHeader(const std::string& filename);
    
private:
    bool writeFileHeader();
    void processingLoop();
    void processVideoFrame(const TimestampedVideoFrame& frame);
    void processAudioPacket(const TimestampedAudioPacket& packet);
    
    uint64_t getTimestampMicros(const std::chrono::high_resolution_clock::time_point& time) const;
    bool shouldDropFrame(const std::chrono::high_resolution_clock::time_point& timestamp) const;
    
    void finalizeCapture();
};

#endif // RAW_CAPTURE_H
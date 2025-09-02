#ifndef VIDEO_STREAM_RECEIVER_H
#define VIDEO_STREAM_RECEIVER_H

#include "stream_receiver.h"
#include "stream_types.h"
#include "config.h"
#include <queue>
#include <mutex>
#include <map>
#include <atomic>

class VideoStreamReceiver : public StreamReceiver {
private:
    // Frame assembly
    struct FrameData {
        std::map<uint16_t, std::vector<uint8_t>> packets; // line_num -> packet_data
        std::chrono::high_resolution_clock::time_point first_packet_time;
        bool complete = false;
    };
    
    std::map<uint16_t, FrameData> frames_in_progress_;
    std::queue<VideoFrame> completed_frames_;
    std::mutex frames_mutex_;
    
    // Statistics
    std::atomic<uint16_t> last_frame_num_;
    std::atomic<size_t> frames_completed_;
    std::atomic<size_t> frames_dropped_;
    std::atomic<size_t> packets_received_;
    
    // VIC-II palette for RGB conversion
    static const uint8_t vic_palette_[16][3];
    
public:
    VideoStreamReceiver();
    ~VideoStreamReceiver() override;
    
    bool initialize() override;
    void start() override;
    void stop() override;
    
    // Frame access
    bool hasFrame() const;
    VideoFrame getNextFrame();
    size_t getFrameQueueSize() const;
    
    // Statistics
    size_t getFramesCompleted() const { return frames_completed_.load(); }
    size_t getFramesDropped() const { return frames_dropped_.load(); }
    size_t getPacketsReceived() const { return packets_received_.load(); }
    
protected:
    void processPacket(const char* data, size_t size) override;
    
private:
    void assembleFrame(uint16_t frame_num, const FrameData& frame_data);
    void convertToRGB(const std::vector<uint8_t>& packet_data, 
                     std::vector<uint8_t>& rgb_output, 
                     int start_line);
    void cleanupOldFrames();
};

#endif // VIDEO_STREAM_RECEIVER_H
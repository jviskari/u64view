#ifndef VIDEO_STREAM_RECEIVER_H
#define VIDEO_STREAM_RECEIVER_H

#include "stream_receiver.h"
#include "config.h"
#include <SDL.h>
#include <queue>
#include <mutex>
#include <vector>
#include <map>
#include <chrono>

struct VideoPacketHeader {
    uint16_t seq;    // Sequence number
    uint16_t frm;    // Frame number
    uint16_t lin;    // Line number
    uint16_t width;  // Width
    uint8_t lp;      // Lines per packet
    uint8_t bp;      // Bytes per line
    uint16_t enc;    // Encoding
};

struct VideoFrame {
    uint16_t frame_num;
    std::vector<uint8_t> rgb_data; // RGB format for SDL
    std::chrono::high_resolution_clock::time_point receive_time;
    bool is_complete;
    
    VideoFrame() : frame_num(0), is_complete(false) {
        rgb_data.resize(Config::Video::FRAME_WIDTH * Config::Video::FRAME_HEIGHT * 3, 0);
    }
};

class VideoStreamReceiver : public StreamReceiver {
private:
    std::queue<VideoFrame> frame_queue;
    std::mutex frame_mutex;
    
    // Frame assembly
    std::map<uint16_t, std::map<uint16_t, std::vector<uint8_t>>> incomplete_frames; // [frame_num][line_num] -> packet_data
    std::mutex assembly_mutex;
    
    // Statistics
    uint16_t last_frame_num;
    size_t dropped_frames;
    size_t total_frames;
    size_t total_packets;
    std::chrono::high_resolution_clock::time_point last_receive_time;
    std::chrono::high_resolution_clock::time_point stats_time;
    
public:
    VideoStreamReceiver();
    ~VideoStreamReceiver();
    
    bool initialize() override;
    void start() override;
    void stop() override;
    
    // Frame access
    bool hasFrame();
    VideoFrame getNextFrame();
    size_t getQueueSize();
    
    // Statistics
    std::chrono::high_resolution_clock::time_point getLastReceiveTime() const { return last_receive_time; }
    size_t getDroppedFrameCount() const { return dropped_frames; }
    size_t getTotalFrameCount() const { return total_frames; }
    size_t getTotalPacketCount() const { return total_packets; }
    
protected:
    void processPacket(const char* data, size_t size) override;
    
private:
    void assembleFrame(uint16_t frame_num, const std::map<uint16_t, std::vector<uint8_t>>& packets);
    void convertPixelsToRGB(const std::vector<uint8_t>& pixel_data, std::vector<uint8_t>& rgb_data, int start_y);
    void cleanupOldFrames();
    void printStats();
};

#endif // VIDEO_STREAM_RECEIVER_H
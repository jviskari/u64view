#include "video_stream_receiver.h"
#include "vic_palette.h"  // Use centralized palette
#include <iostream>
#include <cstring>

VideoStreamReceiver::VideoStreamReceiver()
    : StreamReceiver(Config::Network::MULTICAST_GROUP, Config::Network::VIDEO_PORT),
      last_frame_num_(0), frames_completed_(0), frames_dropped_(0), packets_received_(0) {
}

VideoStreamReceiver::~VideoStreamReceiver() {
    stop();
}

bool VideoStreamReceiver::initialize() {
    return setupMulticastSocket();
}

void VideoStreamReceiver::start() {
    StreamReceiver::start();
    std::cout << "Video stream receiver started on " 
              << Config::Network::MULTICAST_GROUP << ":" 
              << Config::Network::VIDEO_PORT << std::endl;
}

void VideoStreamReceiver::stop() {
    StreamReceiver::stop();
    
    std::lock_guard<std::mutex> lock(frames_mutex_);
    frames_in_progress_.clear();
    while (!completed_frames_.empty()) {
        completed_frames_.pop();
    }
}

void VideoStreamReceiver::processPacket(const char* data, size_t size) {
    if (size < 12) return; // Need at least header
    
    packets_received_++;
    
    // Parse packet header (assuming same format as main.cpp)
    struct PacketHeader {
        uint16_t seq;
        uint16_t frm;
        uint16_t lin;
        uint16_t width;
        uint8_t lp;
        uint8_t bp;
        uint16_t enc;
    } __attribute__((packed));
    
    PacketHeader header;
    std::memcpy(&header, data, sizeof(header));
    
    // Extract pixel data
    std::vector<uint8_t> pixel_data(data + sizeof(header), data + size);
    
    std::lock_guard<std::mutex> lock(frames_mutex_);
    
    // Get or create frame data
    auto& frame_data = frames_in_progress_[header.frm];
    if (frame_data.packets.empty()) {
        frame_data.first_packet_time = std::chrono::high_resolution_clock::now();
    }
    
    // Store packet
    frame_data.packets[header.lin] = pixel_data;
    
    // Check if frame is complete
    if (frame_data.packets.size() >= Config::Video::PACKETS_PER_FRAME) {
        assembleFrame(header.frm, frame_data);
        frames_in_progress_.erase(header.frm);
        frames_completed_++;
    }
    
    // Cleanup old incomplete frames
    cleanupOldFrames();
}

void VideoStreamReceiver::assembleFrame(uint16_t frame_num, const FrameData& frame_data) {
    VideoFrame frame;
    frame.frame_num = frame_num;
    frame.receive_time = frame_data.first_packet_time;
    frame.rgb_data.resize(Config::Video::FRAME_WIDTH * Config::Video::FRAME_HEIGHT * 3);
    
    // Convert packets to RGB
    int line = 0;
    for (const auto& [line_num, packet_data] : frame_data.packets) {
        convertToRGB(packet_data, frame.rgb_data, line * 4); // 4 lines per packet
        line++;
        if (line * 4 >= Config::Video::FRAME_HEIGHT) break;
    }
    
    // Add to completed frames queue
    if (completed_frames_.size() >= Config::Video::MAX_FRAME_QUEUE_SIZE) {
        completed_frames_.pop(); // Remove oldest
        frames_dropped_++;
    }
    completed_frames_.push(frame);
}

void VideoStreamReceiver::convertToRGB(const std::vector<uint8_t>& packet_data, 
                                      std::vector<uint8_t>& rgb_output, 
                                      int start_line) {
    size_t pixel_index = 0;
    
    for (int line = 0; line < 4 && (start_line + line) < Config::Video::FRAME_HEIGHT; line++) {
        for (int x = 0; x < Config::Video::FRAME_WIDTH / 2 && pixel_index < packet_data.size(); x++) {
            uint8_t byte = packet_data[pixel_index++];
            uint8_t pixel1 = byte & 0x0F;
            uint8_t pixel2 = (byte & 0xF0) >> 4;
            
            int y = start_line + line;
            size_t pos1 = static_cast<size_t>(y * Config::Video::FRAME_WIDTH + x * 2) * 3;
            size_t pos2 = static_cast<size_t>(y * Config::Video::FRAME_WIDTH + x * 2 + 1) * 3;
            
            // Set RGB values for pixel 1 using centralized palette
            if (pos1 + 2 < rgb_output.size()) {
                VICIIPalette::getRGBArray(pixel1, &rgb_output[pos1]);
            }
            
            // Set RGB values for pixel 2 using centralized palette
            if (pos2 + 2 < rgb_output.size()) {
                VICIIPalette::getRGBArray(pixel2, &rgb_output[pos2]);
            }
        }
    }
}

void VideoStreamReceiver::cleanupOldFrames() {
    auto now = std::chrono::high_resolution_clock::now();
    auto cutoff = now - std::chrono::milliseconds(200); // Remove frames older than 200ms
    
    for (auto it = frames_in_progress_.begin(); it != frames_in_progress_.end();) {
        if (it->second.first_packet_time < cutoff) {
            it = frames_in_progress_.erase(it);
            frames_dropped_++;
        } else {
            ++it;
        }
    }
}

bool VideoStreamReceiver::hasFrame() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(frames_mutex_));
    return !completed_frames_.empty();
}

VideoFrame VideoStreamReceiver::getNextFrame() {
    std::lock_guard<std::mutex> lock(frames_mutex_);
    if (completed_frames_.empty()) {
        return VideoFrame{}; // Return empty frame
    }
    
    VideoFrame frame = completed_frames_.front();
    completed_frames_.pop();
    return frame;
}

size_t VideoStreamReceiver::getFrameQueueSize() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(frames_mutex_));
    return completed_frames_.size();
}
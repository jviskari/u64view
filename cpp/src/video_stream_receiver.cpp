#include "video_stream_receiver.h"
#include "vic_palette.h"
#include <iostream>
#include <cstring>
#include <algorithm>

VideoStreamReceiver::VideoStreamReceiver()
    : StreamReceiver(Config::Network::MULTICAST_GROUP, Config::Network::VIDEO_PORT),
      last_frame_num(0), dropped_frames(0), total_frames(0), total_packets(0) {
}

VideoStreamReceiver::~VideoStreamReceiver() {
    stop();
}

bool VideoStreamReceiver::initialize() {
    if (!setupMulticastSocket()) {
        return false;
    }
    
    std::cout << "Video receiver initialized for " << Config::Video::FRAME_WIDTH 
              << "x" << Config::Video::FRAME_HEIGHT << " @ " << Config::Video::FRAME_RATE << "Hz" << std::endl;
    
    stats_time = std::chrono::high_resolution_clock::now();
    return true;
}

void VideoStreamReceiver::start() {
    StreamReceiver::start();
    std::cout << "Video receiver started" << std::endl;
}

void VideoStreamReceiver::stop() {
    StreamReceiver::stop();
    
    // Clear queues
    std::lock_guard<std::mutex> frame_lock(frame_mutex);
    std::lock_guard<std::mutex> assembly_lock(assembly_mutex);
    
    while (!frame_queue.empty()) {
        frame_queue.pop();
    }
    incomplete_frames.clear();
}

void VideoStreamReceiver::setLastReceiveTime(const std::chrono::high_resolution_clock::time_point& time) {
    std::lock_guard<std::mutex> lock(time_mutex);
    last_receive_time = time;
}

std::chrono::high_resolution_clock::time_point VideoStreamReceiver::getLastReceiveTime() const {
    std::lock_guard<std::mutex> lock(time_mutex);
    return last_receive_time;
}

void VideoStreamReceiver::processPacket(const char* data, size_t size) {
    if (size < sizeof(VideoPacketHeader)) return;
    
    total_packets++;
    setLastReceiveTime(std::chrono::high_resolution_clock::now());
    
    // Parse header
    VideoPacketHeader header;
    std::memcpy(&header, data, sizeof(header));
    
    // Extract pixel data (skip header)
    const char* pixel_data = data + sizeof(VideoPacketHeader);
    size_t pixel_data_size = size - sizeof(VideoPacketHeader);
    
    if (pixel_data_size == 0) return;
    
    // Store packet data for frame assembly
    {
        std::lock_guard<std::mutex> lock(assembly_mutex);
        
        // Store packet data
        std::vector<uint8_t> packet_pixels(pixel_data, pixel_data + pixel_data_size);
        incomplete_frames[header.frm][header.lin] = packet_pixels;
        
        // Check if frame is complete (expecting 68 packets per frame)
        if (incomplete_frames[header.frm].size() >= Config::Video::PACKETS_PER_FRAME) {
            assembleFrame(header.frm, incomplete_frames[header.frm]);
            incomplete_frames.erase(header.frm);
        }
        
        // Clean up old incomplete frames periodically
        if (total_packets % 100 == 0) {
            cleanupOldFrames();
        }
    }
    
    // Print stats periodically
    if (Config::Debug::SHOW_PACKET_STATS) {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - stats_time);
        if (elapsed.count() >= Config::Debug::STATS_UPDATE_INTERVAL_MS) {
            printStats();
            stats_time = now;
        }
    }
}

void VideoStreamReceiver::assembleFrame(uint16_t frame_num, const std::map<uint16_t, std::vector<uint8_t>>& packets) {
    VideoFrame frame;
    frame.frame_num = frame_num;
    frame.receive_time = std::chrono::high_resolution_clock::now();
    
    // Sort packets by line number and convert to RGB
    int current_y = 0;
    for (const auto& [line_num, packet_data] : packets) {
        if (current_y >= Config::Video::FRAME_HEIGHT) break;
        
        convertPixelsToRGB(packet_data, frame.rgb_data, current_y);
        current_y += 4; // Each packet contains 4 lines
    }
    
    frame.is_complete = (current_y >= Config::Video::FRAME_HEIGHT);
    
    // Check for dropped frames
    if (total_frames > 0) {
        uint16_t expected_next = (last_frame_num + 1) & 0xFFFF;
        if (frame_num != expected_next && frame_num != last_frame_num) {
            dropped_frames++;
            if (Config::Debug::SHOW_PACKET_STATS) {
                std::cout << "F" << std::flush; // Frame drop indicator
            }
        }
    }
    last_frame_num = frame_num;
    total_frames++;
    
    // Add to queue
    {
        std::lock_guard<std::mutex> lock(frame_mutex);
        if (frame_queue.size() >= Config::Video::MAX_FRAME_QUEUE_SIZE) {
            frame_queue.pop(); // Remove oldest frame
        }
        frame_queue.push(frame);
    }
}

void VideoStreamReceiver::convertPixelsToRGB(const std::vector<uint8_t>& pixel_data, std::vector<uint8_t>& rgb_data, int start_y) {
    size_t pixel_index = 0;
    
    // Each packet contains 4 lines of data
    for (int line = 0; line < 4 && (start_y + line) < Config::Video::FRAME_HEIGHT; ++line) {
        int y = start_y + line;
        
        // Each line has 192 bytes (384 pixels / 2 pixels per byte)
        for (int x = 0; x < 192 && x < Config::Video::FRAME_WIDTH / 2; ++x) {
            if (pixel_index >= pixel_data.size()) break;
            
            uint8_t pixel_byte = pixel_data[pixel_index++];
            
            // Extract two 4-bit pixels and convert to RGB
            VICIIPalette::RGB pixel1, pixel2;
            VICIIPalette::unpackPixelPair(pixel_byte, pixel1, pixel2);
            
            // Calculate RGB buffer positions - fix sign comparison warnings
            size_t pos1 = static_cast<size_t>(y * Config::Video::FRAME_WIDTH + x * 2) * 3;
            size_t pos2 = static_cast<size_t>(y * Config::Video::FRAME_WIDTH + x * 2 + 1) * 3;
            
            // Store RGB data with proper bounds checking
            if (pos1 + 2 < rgb_data.size()) {
                rgb_data[pos1] = pixel1.r;
                rgb_data[pos1 + 1] = pixel1.g;
                rgb_data[pos1 + 2] = pixel1.b;
            }
            
            if (pos2 + 2 < rgb_data.size()) {
                rgb_data[pos2] = pixel2.r;
                rgb_data[pos2 + 1] = pixel2.g;
                rgb_data[pos2 + 2] = pixel2.b;
            }
        }
    }
}

bool VideoStreamReceiver::hasFrame() {
    std::lock_guard<std::mutex> lock(frame_mutex);
    return !frame_queue.empty();
}

VideoFrame VideoStreamReceiver::getNextFrame() {
    std::lock_guard<std::mutex> lock(frame_mutex);
    if (frame_queue.empty()) {
        return VideoFrame(); // Return empty frame
    }
    
    VideoFrame frame = frame_queue.front();
    frame_queue.pop();
    return frame;
}

size_t VideoStreamReceiver::getQueueSize() {
    std::lock_guard<std::mutex> lock(frame_mutex);
    return frame_queue.size();
}

void VideoStreamReceiver::cleanupOldFrames() {
    // Remove incomplete frames that are too old (more than 10 frames behind)
    if (incomplete_frames.empty()) return;
    
    // Fix: Remove unused variable warning by using the cutoff value
    uint16_t cutoff_age = 10; // Frames to keep
    
    auto it = incomplete_frames.begin();
    while (it != incomplete_frames.end()) {
        uint16_t frame_age = (last_frame_num - it->first) & 0xFFFF;
        if (frame_age > cutoff_age && frame_age < 32768) { // Handle wrap-around
            it = incomplete_frames.erase(it);
        } else {
            ++it;
        }
    }
}

void VideoStreamReceiver::printStats() {
    double frame_drop_rate = total_frames > 0 ? (100.0 * dropped_frames / total_frames) : 0.0;
    std::cout << "\nVideo Stats - Frames: " << total_frames 
              << ", Dropped: " << dropped_frames 
              << " (" << frame_drop_rate << "%), Queue: " << getQueueSize()
              << ", Incomplete: " << incomplete_frames.size() << std::endl;
}
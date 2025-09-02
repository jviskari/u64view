#ifndef STREAM_TYPES_H
#define STREAM_TYPES_H

#include <vector>
#include <chrono>
#include <cstdint>

// Video frame structure
struct VideoFrame {
    uint16_t frame_num;
    std::vector<uint8_t> rgb_data; // RGB24 format: width * height * 3
    std::chrono::high_resolution_clock::time_point receive_time;
    
    VideoFrame() : frame_num(0) {
        rgb_data.resize(384 * 272 * 3, 0); // Default C64 resolution
    }
    
    VideoFrame(uint16_t num, const std::vector<uint8_t>& data) 
        : frame_num(num), rgb_data(data), 
          receive_time(std::chrono::high_resolution_clock::now()) {}
};

// Audio packet structure  
struct AudioPacket {
    uint16_t sequence;
    std::vector<int16_t> samples; // 16-bit signed samples
    std::chrono::high_resolution_clock::time_point receive_time;
    
    AudioPacket() : sequence(0), 
                   receive_time(std::chrono::high_resolution_clock::now()) {}
    
    AudioPacket(uint16_t seq, const std::vector<int16_t>& data)
        : sequence(seq), samples(data),
          receive_time(std::chrono::high_resolution_clock::now()) {}
};

#endif // STREAM_TYPES_H
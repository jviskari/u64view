#include "av_frame_serialization.h"
#include <sstream>
#include <iostream>
#include <cassert>

void testVideoFrameSerialization() {
    std::cout << "Testing VideoAVFrame serialization..." << std::endl;
    
    // Create test video frame
    VideoAVFrame original;
    original.frame_number = 12345;
    original.timestamp_us = 1000000;
    original.presentation_time_us = 1020000;
    original.is_complete = true;
    original.width = 384;
    original.height = 272;
    original.rgb_data = {255, 0, 0, 0, 255, 0, 0, 0, 255}; // RGB test pattern
    
    // Serialize to memory
    std::stringstream buffer;
    bool serialize_ok = AVFrameSerializer::serialize(buffer, original);
    assert(serialize_ok);
    std::cout << "  Serialization: OK" << std::endl;
    
    // Deserialize back
    buffer.seekg(0);
    auto deserialized = AVFrameSerializer::deserialize(buffer);
    assert(deserialized != nullptr);
    assert(deserialized->frame_type == AVFrameType::VIDEO);
    std::cout << "  Deserialization: OK" << std::endl;
    
    auto video_frame = static_cast<VideoAVFrame*>(deserialized.get());
    
    // Verify all fields match exactly
    assert(video_frame->frame_number == original.frame_number);
    assert(video_frame->timestamp_us == original.timestamp_us);
    assert(video_frame->presentation_time_us == original.presentation_time_us);
    assert(video_frame->is_complete == original.is_complete);
    assert(video_frame->width == original.width);
    assert(video_frame->height == original.height);
    assert(video_frame->rgb_data == original.rgb_data);
    
    std::cout << "  Round-trip verification: OK" << std::endl;
    std::cout << "VideoAVFrame serialization test PASSED!" << std::endl;
}

void testAudioFrameSerialization() {
    std::cout << "Testing AudioAVFrame serialization..." << std::endl;
    
    // Create test audio frame
    AudioAVFrame original;
    original.frame_number = 67890;
    original.timestamp_us = 2000000;
    original.presentation_time_us = 2000000;
    original.is_complete = true;
    original.sample_rate = 48000;
    original.channels = 2;
    original.samples = {1000, -1000, 2000, -2000, 3000, -3000}; // Test pattern
    
    // Serialize to memory
    std::stringstream buffer;
    bool serialize_ok = AVFrameSerializer::serialize(buffer, original);
    assert(serialize_ok);
    std::cout << "  Serialization: OK" << std::endl;
    
    // Deserialize back
    buffer.seekg(0);
    auto deserialized = AVFrameSerializer::deserialize(buffer);
    assert(deserialized != nullptr);
    assert(deserialized->frame_type == AVFrameType::AUDIO);
    std::cout << "  Deserialization: OK" << std::endl;
    
    auto audio_frame = static_cast<AudioAVFrame*>(deserialized.get());
    
    // Verify all fields match exactly
    assert(audio_frame->frame_number == original.frame_number);
    assert(audio_frame->timestamp_us == original.timestamp_us);
    assert(audio_frame->presentation_time_us == original.presentation_time_us);
    assert(audio_frame->is_complete == original.is_complete);
    assert(audio_frame->sample_rate == original.sample_rate);
    assert(audio_frame->channels == original.channels);
    assert(audio_frame->samples == original.samples);
    
    std::cout << "  Round-trip verification: OK" << std::endl;
    std::cout << "AudioAVFrame serialization test PASSED!" << std::endl;
}

int main() {
    std::cout << "=== AVFrame Serialization Tests ===" << std::endl;
    
    try {
        testVideoFrameSerialization();
        std::cout << std::endl;
        testAudioFrameSerialization();
        std::cout << std::endl;
        std::cout << "All tests PASSED!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
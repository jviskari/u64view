#include "file_av_recorder.h"
#include "file_av_producer.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include <filesystem>

// Test consumer that collects frames for verification
class TestConsumer : public AVStreamConsumer {
public:
    std::vector<VideoAVFrame> video_frames;
    std::vector<AudioAVFrame> audio_frames;
    bool stream_ended = false;
    std::string last_error;
    
    void onVideoFrame(const VideoAVFrame& frame) override {
        video_frames.push_back(frame);
        std::cout << "Received video frame " << frame.frame_number << std::endl;
    }
    
    void onAudioFrame(const AudioAVFrame& frame) override {
        audio_frames.push_back(frame);
        std::cout << "Received audio frame " << frame.frame_number << std::endl;
    }
    
    void onStreamEnd() override {
        stream_ended = true;
        std::cout << "Stream ended" << std::endl;
    }
    
    void onStreamError(const std::string& error) override {
        last_error = error;
        std::cerr << "Stream error: " << error << std::endl;
    }
};

void testFileRoundTrip() {
    const std::string test_filename = "test_roundtrip.u64";
    
    std::cout << "=== File Round-Trip Test ===" << std::endl;
    
    // Clean up any existing test file
    if (std::filesystem::exists(test_filename)) {
        std::filesystem::remove(test_filename);
    }
    
    // Create test frames
    std::vector<VideoAVFrame> original_video_frames;
    std::vector<AudioAVFrame> original_audio_frames;
    
    // Create 3 test video frames
    for (int i = 0; i < 3; i++) {
        VideoAVFrame frame;
        frame.frame_number = i;
        frame.timestamp_us = i * 20000; // 20ms apart
        frame.presentation_time_us = frame.timestamp_us;
        frame.is_complete = true;
        frame.width = 384;
        frame.height = 272;
        
        // Create test pattern
        frame.rgb_data.resize(frame.width * frame.height * 3);
        for (size_t j = 0; j < frame.rgb_data.size(); j += 3) {
            frame.rgb_data[j] = (i * 50) % 256;     // R
            frame.rgb_data[j + 1] = (i * 100) % 256; // G
            frame.rgb_data[j + 2] = (i * 150) % 256; // B
        }
        
        original_video_frames.push_back(frame);
    }
    
    // Create 3 test audio frames
    for (int i = 0; i < 3; i++) {
        AudioAVFrame frame;
        frame.frame_number = i + 100; // Different numbering
        frame.timestamp_us = i * 10000; // 10ms apart
        frame.presentation_time_us = frame.timestamp_us;
        frame.is_complete = true;
        frame.sample_rate = 48000;
        frame.channels = 2;
        
        // Create test audio pattern
        frame.samples.resize(1024); // 1024 samples
        for (size_t j = 0; j < frame.samples.size(); j++) {
            frame.samples[j] = static_cast<int16_t>((i * 1000 + j) % 32767);
        }
        
        original_audio_frames.push_back(frame);
    }
    
    // Phase 1: Record frames to file
    std::cout << "\nPhase 1: Recording frames to file..." << std::endl;
    {
        FileAVRecorder recorder(test_filename);
        assert(recorder.startRecording());
        
        // Send all frames to recorder
        for (const auto& frame : original_video_frames) {
            recorder.onVideoFrame(frame);
        }
        for (const auto& frame : original_audio_frames) {
            recorder.onAudioFrame(frame);
        }
        
        recorder.stopRecording();
        std::cout << "Recorded " << recorder.getFrameCount() << " frames" << std::endl;
    }
    
    // Verify file was created
    assert(std::filesystem::exists(test_filename));
    auto file_size = std::filesystem::file_size(test_filename);
    std::cout << "Created file size: " << file_size << " bytes" << std::endl;
    assert(file_size > 0);
    
    // Phase 2: Read frames back from file
    std::cout << "\nPhase 2: Reading frames back from file..." << std::endl;
    {
        FileAVProducer producer(test_filename);
        auto consumer = std::make_shared<TestConsumer>();
        
        assert(producer.initialize());
        producer.addConsumer(consumer);
        producer.start();
        
        // Wait for playback to complete
        while (!consumer->stream_ended && consumer->last_error.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        producer.stop();
        
        std::cout << "Read back " << consumer->video_frames.size() 
                  << " video frames and " << consumer->audio_frames.size() 
                  << " audio frames" << std::endl;
        
        // Verify we got the right number of frames
        assert(consumer->video_frames.size() == original_video_frames.size());
        assert(consumer->audio_frames.size() == original_audio_frames.size());
        assert(consumer->last_error.empty());
        
        // Verify video frames match exactly
        for (size_t i = 0; i < original_video_frames.size(); i++) {
            const auto& orig = original_video_frames[i];
            const auto& read = consumer->video_frames[i];
            
            assert(read.frame_number == orig.frame_number);
            assert(read.timestamp_us == orig.timestamp_us);
            assert(read.presentation_time_us == orig.presentation_time_us);
            assert(read.is_complete == orig.is_complete);
            assert(read.width == orig.width);
            assert(read.height == orig.height);
            assert(read.rgb_data == orig.rgb_data);
        }
        
        // Verify audio frames match exactly
        for (size_t i = 0; i < original_audio_frames.size(); i++) {
            const auto& orig = original_audio_frames[i];
            const auto& read = consumer->audio_frames[i];
            
            assert(read.frame_number == orig.frame_number);
            assert(read.timestamp_us == orig.timestamp_us);
            assert(read.presentation_time_us == orig.presentation_time_us);
            assert(read.is_complete == orig.is_complete);
            assert(read.sample_rate == orig.sample_rate);
            assert(read.channels == orig.channels);
            assert(read.samples == orig.samples);
        }
    }
    
    // Clean up
    std::filesystem::remove(test_filename);
    
    std::cout << "\nFile round-trip test PASSED!" << std::endl;
    std::cout << "All frames were recorded and read back identically." << std::endl;
}

int main() {
    try {
        testFileRoundTrip();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
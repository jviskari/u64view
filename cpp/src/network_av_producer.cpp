#include "network_av_producer.h"
#include "config.h"
#include <iostream>
#include <algorithm>
#include <queue>

NetworkAVProducer::NetworkAVProducer() 
    : running(false), next_video_frame_number(0), next_audio_frame_number(0),
      video_frames_produced(0), audio_frames_produced(0) {
}

NetworkAVProducer::~NetworkAVProducer() {
    stop();
}

bool NetworkAVProducer::initialize() {
    // Initialize video receiver
    video_receiver = std::make_unique<VideoStreamReceiver>();
    if (!video_receiver->initialize()) {
        std::cerr << "Failed to initialize video receiver" << std::endl;
        return false;
    }
    
    // Initialize audio receiver (no SDL here!)
    audio_receiver = std::make_unique<AudioStreamReceiver>();
    if (!audio_receiver->initialize()) {
        std::cerr << "Failed to initialize audio receiver" << std::endl;
        return false;
    }
    
    // Initialize synchronizer
    av_sync = std::make_unique<AVSynchronizer>();
    
    start_time = std::chrono::high_resolution_clock::now();
    last_stats_time = start_time;
    
    std::cout << "Network AV Producer initialized" << std::endl;
    std::cout << "Video: " << Config::Network::MULTICAST_GROUP << ":" << Config::Network::VIDEO_PORT << std::endl;
    std::cout << "Audio: " << Config::Network::MULTICAST_GROUP << ":" << Config::Network::AUDIO_PORT << std::endl;
    
    return true;
}

void NetworkAVProducer::start() {
    if (running.load()) {
        std::cout << "NetworkAVProducer already running" << std::endl;
        return;
    }
    
    // Start network receivers
    video_receiver->start();
    audio_receiver->start();
    
    // Start producer thread
    running = true;
    producer_thread = std::thread(&NetworkAVProducer::producerLoop, this);
    
    std::cout << "Network AV Producer started" << std::endl;
}

void NetworkAVProducer::stop() {
    running = false;
    
    if (producer_thread.joinable()) {
        producer_thread.join();
    }
    
    if (video_receiver) {
        video_receiver->stop();
    }
    
    if (audio_receiver) {
        audio_receiver->stop();
    }
    
    std::cout << "Network AV Producer stopped" << std::endl;
}

void NetworkAVProducer::addConsumer(std::shared_ptr<AVStreamConsumer> consumer) {
    std::lock_guard<std::mutex> lock(consumers_mutex);
    consumers.push_back(consumer);
    std::cout << "Added consumer to network producer" << std::endl;
}

void NetworkAVProducer::removeConsumer(std::shared_ptr<AVStreamConsumer> consumer) {
    std::lock_guard<std::mutex> lock(consumers_mutex);
    consumers.erase(
        std::remove(consumers.begin(), consumers.end(), consumer),
        consumers.end()
    );
    std::cout << "Removed consumer from network producer" << std::endl;
}

std::chrono::microseconds NetworkAVProducer::getPosition() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time);
    return elapsed;
}

void NetworkAVProducer::producerLoop() {
    std::cout << "Starting network producer loop" << std::endl;
    
    // Frame timing for 50Hz
    const auto frame_duration = std::chrono::microseconds(20000); // 20ms = 50Hz
    auto next_frame_time = std::chrono::high_resolution_clock::now();
    
    // Keep a small buffer of frames to smooth out network jitter
    std::queue<VideoFrame> frame_buffer;
    const size_t BUFFER_SIZE = 2; // Buffer 2 frames (40ms)
    
    while (running.load()) {
        auto now = std::chrono::high_resolution_clock::now();
        
        // Fill frame buffer
        while (frame_buffer.size() < BUFFER_SIZE && video_receiver->hasFrame()) {
            VideoFrame video_frame = video_receiver->getNextFrame();
            if (video_frame.is_complete) {
                frame_buffer.push(video_frame);
            }
        }
        
        // Process video frame at correct timing
        if (now >= next_frame_time && !frame_buffer.empty()) {
            VideoFrame video_frame = frame_buffer.front();
            frame_buffer.pop();
            
            // Convert with proper timing
            VideoAVFrame av_frame = convertVideoFrame(video_frame);
            
            // Override timestamps with correct presentation time
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                next_frame_time - start_time);
            av_frame.timestamp_us = elapsed.count();
            av_frame.presentation_time_us = av_frame.timestamp_us;
            
            // Send to all consumers
            {
                std::lock_guard<std::mutex> lock(consumers_mutex);
                for (auto& consumer : consumers) {
                    consumer->onVideoFrame(av_frame);
                }
            }
            
            video_frames_produced++;
            av_sync->updateVideoTimestamp();
            av_sync->frameDisplayed(av_frame.frame_number);
            
            // Schedule next frame
            next_frame_time += frame_duration;
            
            // If we're running behind, catch up
            if (next_frame_time < now - frame_duration) {
                next_frame_time = now;
            }
        }
        
        // Process audio continuously
        if (audio_receiver->hasAudioData()) {
            const size_t samples_per_frame = 1024; // About 21ms at 48kHz
            auto audio_samples = audio_receiver->getNextSamples(samples_per_frame);
            
            if (!audio_samples.empty()) {
                auto timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    now - start_time).count();
                
                AudioAVFrame av_frame = convertAudioFrame(audio_samples, timestamp_us);
                
                // Send to all consumers
                {
                    std::lock_guard<std::mutex> lock(consumers_mutex);
                    for (auto& consumer : consumers) {
                        consumer->onAudioFrame(av_frame);
                    }
                }
                
                audio_frames_produced++;
                av_sync->updateAudioTimestamp();
            }
        }
        
        // Print stats periodically
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_time).count() >= 5) {
            printStats();
            last_stats_time = now;
        }
        
        // Sleep until next frame time or 1ms, whichever is shorter
        auto sleep_time = next_frame_time - now;
        if (sleep_time > std::chrono::milliseconds(1)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    std::cout << "Network producer loop finished" << std::endl;
}

VideoAVFrame NetworkAVProducer::convertVideoFrame(const VideoFrame& frame) {
    VideoAVFrame av_frame;
    av_frame.frame_type = AVFrameType::VIDEO;
    av_frame.frame_number = next_video_frame_number++;
    
    // Calculate timestamp based on frame receive time
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        frame.receive_time - start_time);
    av_frame.timestamp_us = elapsed.count();
    av_frame.presentation_time_us = av_frame.timestamp_us;
    
    av_frame.is_complete = frame.is_complete;
    av_frame.width = Config::Video::FRAME_WIDTH;
    av_frame.height = Config::Video::FRAME_HEIGHT;
    
    // Copy RGB data directly (VideoFrame already has RGB data)
    av_frame.rgb_data = frame.rgb_data;
    
    return av_frame;
}

AudioAVFrame NetworkAVProducer::convertAudioFrame(const std::vector<int16_t>& samples, uint64_t timestamp_us) {
    AudioAVFrame av_frame;
    av_frame.frame_type = AVFrameType::AUDIO;
    av_frame.frame_number = next_audio_frame_number++;
    av_frame.timestamp_us = timestamp_us;
    av_frame.presentation_time_us = timestamp_us;
    av_frame.is_complete = true;
    av_frame.sample_rate = Config::Audio::SAMPLE_RATE;
    av_frame.channels = Config::Audio::CHANNELS;
    
    // Copy sample data
    av_frame.samples = samples;
    
    return av_frame;
}

void NetworkAVProducer::notifyError(const std::string& error) {
    std::lock_guard<std::mutex> lock(consumers_mutex);
    for (auto& consumer : consumers) {
        consumer->onStreamError(error);
    }
}

void NetworkAVProducer::printStats() {
    std::cout << "\n=== Network Producer Stats ===" << std::endl;
    std::cout << "Video frames: " << video_frames_produced.load() << std::endl;
    std::cout << "Audio frames: " << audio_frames_produced.load() << std::endl;
    
    if (video_receiver) {
        std::cout << "Video receiver - Total: " << video_receiver->getTotalFrameCount()
                  << ", Dropped: " << video_receiver->getDroppedFrameCount()
                  << ", Queue: " << video_receiver->getQueueSize() << std::endl;
    }
    
    if (audio_receiver) {
        std::cout << "Audio receiver - Total: " << audio_receiver->getTotalPacketCount()
                  << ", Dropped: " << audio_receiver->getDroppedPacketCount()
                  << ", Queue: " << audio_receiver->getQueueSize() << std::endl;
    }
    
    if (av_sync && av_sync->isInitialized()) {
        std::cout << "A/V Sync - Drift: " << av_sync->getAVDriftMs() << "ms"
                  << ", In Sync: " << (av_sync->isInSync() ? "YES" : "NO") << std::endl;
    }
}
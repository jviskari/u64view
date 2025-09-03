#include "file_av_producer.h"
#include "u64_format.h"
#include <iostream>
#include <cstring>

FileAVProducer::FileAVProducer(const std::string& filename) 
    : filename(filename), running(false), paused(false), frames_read(0) {}

FileAVProducer::~FileAVProducer() {
    stop();
}

bool FileAVProducer::initialize() {
    file.open(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return false;
    }
    
    // Skip header if present and remember data start position
    if (!skipHeader()) {
        std::cerr << "Invalid file format: " << filename << std::endl;
        file.close();
        return false;
    }
    
    data_start_pos = file.tellg();
    
    std::cout << "Opened file: " << filename << std::endl;
    return true;
}

void FileAVProducer::start() {
    if (running.load()) {
        std::cout << "FileAVProducer already running" << std::endl;
        return;
    }
    
    running = true;
    frames_read = 0;
    playback_start_time = std::chrono::high_resolution_clock::now();
    playback_thread = std::thread(&FileAVProducer::playbackLoop, this);
    std::cout << "Started file playback: " << filename << std::endl;
}

void FileAVProducer::stop() {
    running = false;
    if (playback_thread.joinable()) {
        playback_thread.join();
    }
    if (file.is_open()) {
        file.close();
    }
    std::cout << "Stopped file playback: " << filename << std::endl;
}

void FileAVProducer::addConsumer(std::shared_ptr<AVStreamConsumer> consumer) {
    std::lock_guard<std::mutex> lock(consumers_mutex);
    consumers.push_back(consumer);
    std::cout << "Added consumer to file producer" << std::endl;
}

void FileAVProducer::removeConsumer(std::shared_ptr<AVStreamConsumer> consumer) {
    std::lock_guard<std::mutex> lock(consumers_mutex);
    consumers.erase(
        std::remove(consumers.begin(), consumers.end(), consumer),
        consumers.end()
    );
    std::cout << "Removed consumer from file producer" << std::endl;
}

void FileAVProducer::playbackLoop() {
    std::cout << "Starting playback loop for: " << filename << std::endl;
    
    // Frame timing for 50Hz video
    const auto frame_duration = std::chrono::microseconds(20000); // 20ms = 50Hz
    auto next_video_frame_time = playback_start_time;
    uint64_t video_frames_sent = 0;
    
    // Read-ahead buffer for frames
    std::queue<std::unique_ptr<AVFrame>> frame_buffer;
    const size_t BUFFER_SIZE = 10; // Buffer up to 10 frames
    
    while (running.load()) {
        // Check if we're at end of file before trying to read
        if (file.eof() || file.peek() == EOF) {
            // If buffer is empty, we're done
            if (frame_buffer.empty()) {
                std::cout << "Reached end of file: " << filename << std::endl;
                break;
            }
        }
        
        if (paused.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        // Fill buffer with frames
        while (frame_buffer.size() < BUFFER_SIZE && !file.eof() && file.peek() != EOF) {
            auto pos_before = file.tellg();
            auto frame = AVFrameSerializer::deserialize(file);
            
            if (!frame) {
                if (file.eof()) {
                    break; // Normal end of file
                } else {
                    std::cerr << "Failed to deserialize frame at position " << pos_before 
                              << " in file: " << filename << std::endl;
                    notifyError("Failed to deserialize frame");
                    break;
                }
            }
            
            frames_read++;
            frame_buffer.push(std::move(frame));
        }
        
        // Process buffered frames at correct timing
        auto now = std::chrono::high_resolution_clock::now();
        
        while (!frame_buffer.empty()) {
            auto& frame = frame_buffer.front();
            
            // Handle video frames with proper timing
            if (frame->frame_type == AVFrameType::VIDEO) {
                // Wait until it's time to send this frame
                if (now < next_video_frame_time) {
                    break; // Not time yet, keep frame in buffer
                }
                
                // Update presentation timestamp to match playback timing
                auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                    next_video_frame_time - playback_start_time);
                frame->presentation_time_us = elapsed.count();
                
                // Send to consumers
                {
                    std::lock_guard<std::mutex> lock(consumers_mutex);
                    auto video_frame = static_cast<VideoAVFrame*>(frame.get());
                    for (auto& consumer : consumers) {
                        consumer->onVideoFrame(*video_frame);
                    }
                }
                
                video_frames_sent++;
                next_video_frame_time += frame_duration;
                
                // If we're running behind, catch up
                if (next_video_frame_time < now - frame_duration * 2) {
                    next_video_frame_time = now;
                }
            } 
            // Handle audio frames immediately (they have their own timing via the audio callback)
            else if (frame->frame_type == AVFrameType::AUDIO) {
                std::lock_guard<std::mutex> lock(consumers_mutex);
                auto audio_frame = static_cast<AudioAVFrame*>(frame.get());
                for (auto& consumer : consumers) {
                    consumer->onAudioFrame(*audio_frame);
                }
            }
            
            frame_buffer.pop();
        }
        
        // Calculate sleep time until next video frame
        now = std::chrono::high_resolution_clock::now();
        auto sleep_time = next_video_frame_time - now;
        if (sleep_time > std::chrono::milliseconds(1)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    std::cout << "Playback loop finished. Read " << frames_read.load() 
              << " frames, sent " << video_frames_sent << " video frames" << std::endl;
    notifyStreamEnd();
}

bool FileAVProducer::skipHeader() {
    // Check if file starts with our magic header
    U64Format::SimpleHeader header;
    auto pos = file.tellg();
    
    if (file.read(reinterpret_cast<char*>(&header), sizeof(header))) {
        if (std::memcmp(header.magic, U64Format::MAGIC, 8) == 0) {
            // Valid header found, continue from current position
            std::cout << "Found U64 header, version " << header.version << std::endl;
            return true;
        }
    }
    
    // No header or invalid header - assume raw AVFrame stream, reset to beginning
    std::cout << "No U64 header found, assuming raw AVFrame stream" << std::endl;
    file.clear();
    file.seekg(pos);
    return true;
}

void FileAVProducer::notifyError(const std::string& error) {
    std::lock_guard<std::mutex> lock(consumers_mutex);
    for (auto& consumer : consumers) {
        consumer->onStreamError(error);
    }
}

void FileAVProducer::notifyStreamEnd() {
    std::lock_guard<std::mutex> lock(consumers_mutex);
    for (auto& consumer : consumers) {
        consumer->onStreamEnd();
    }
}

std::chrono::microseconds FileAVProducer::getPosition() const {
    if (!running.load()) {
        return std::chrono::microseconds::zero();
    }
    
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        now - playback_start_time);
    return elapsed;
}
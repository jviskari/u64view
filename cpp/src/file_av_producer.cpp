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
    
    while (running.load()) {
        // Check if we're at end of file before trying to read
        if (file.eof() || file.peek() == EOF) {
            std::cout << "Reached end of file: " << filename << std::endl;
            break;
        }
        
        if (paused.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        // Save current position for error reporting
        auto pos_before = file.tellg();
        
        // Deserialize the next AVFrame
        auto frame = AVFrameSerializer::deserialize(file);
        if (!frame) {
            // Check if we hit EOF naturally
            if (file.eof()) {
                std::cout << "Reached end of file: " << filename << std::endl;
                break;
            } else {
                // This is a real error
                std::cerr << "Failed to deserialize frame at position " << pos_before 
                          << " in file: " << filename << std::endl;
                notifyError("Failed to deserialize frame");
                break;
            }
        }
        
        frames_read++;
        
        // Distribute to consumers based on type
        std::lock_guard<std::mutex> lock(consumers_mutex);
        if (frame->frame_type == AVFrameType::VIDEO) {
            auto video_frame = static_cast<VideoAVFrame*>(frame.get());
            for (auto& consumer : consumers) {
                consumer->onVideoFrame(*video_frame);
            }
        } else if (frame->frame_type == AVFrameType::AUDIO) {
            auto audio_frame = static_cast<AudioAVFrame*>(frame.get());
            for (auto& consumer : consumers) {
                consumer->onAudioFrame(*audio_frame);
            }
        }
        
        // Simple timing - could be improved with actual timestamp-based timing
        // For now, just a small delay to prevent overwhelming consumers
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    std::cout << "Playback loop finished. Read " << frames_read.load() << " frames" << std::endl;
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
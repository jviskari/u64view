#include "file_av_recorder.h"
#include "u64_format.h"
#include <iostream>
#include <cstring>

FileAVRecorder::FileAVRecorder(const std::string& filename) 
    : filename(filename), recording(false), frame_count(0) {}

FileAVRecorder::~FileAVRecorder() {
    stopRecording();
}

bool FileAVRecorder::startRecording() {
    if (recording.load()) {
        std::cerr << "Already recording to: " << filename << std::endl;
        return false;
    }
    
    file.open(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for recording: " << filename << std::endl;
        return false;
    }
    
    // Write minimal header (optional)
    U64Format::SimpleHeader header = {};
    std::memcpy(header.magic, U64Format::MAGIC, 8);
    header.version = U64Format::VERSION;
    header.reserved = 0;
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    
    frame_count = 0;
    recording = true;
    
    std::cout << "Started recording to: " << filename << std::endl;
    return true;
}

void FileAVRecorder::onVideoFrame(const VideoAVFrame& frame) {
    if (!recording.load()) return;
    
    std::lock_guard<std::mutex> lock(file_mutex);
    if (AVFrameSerializer::serialize(file, frame)) {
        frame_count++;
    } else {
        std::cerr << "Failed to serialize video frame " << frame.frame_number << std::endl;
    }
}

void FileAVRecorder::onAudioFrame(const AudioAVFrame& frame) {
    if (!recording.load()) return;
    
    std::lock_guard<std::mutex> lock(file_mutex);
    if (AVFrameSerializer::serialize(file, frame)) {
        frame_count++;
    } else {
        std::cerr << "Failed to serialize audio frame " << frame.frame_number << std::endl;
    }
}

void FileAVRecorder::stopRecording() {
    if (!recording.load()) return;
    
    recording = false;
    
    std::lock_guard<std::mutex> lock(file_mutex);
    if (file.is_open()) {
        file.close();
    }
    
    std::cout << "Recording stopped. Saved " << frame_count.load() 
              << " frames to " << filename << std::endl;
}

void FileAVRecorder::onStreamEnd() {
    std::cout << "Stream ended, stopping recording" << std::endl;
    stopRecording();
}

void FileAVRecorder::onStreamError(const std::string& error) {
    std::cerr << "Stream error during recording: " << error << std::endl;
    stopRecording();
}
#include "raw_capture.h"
#include "config.h"
#include <iostream>
#include <filesystem>
#include <cstring>

RawCapture::RawCapture() : sync_enabled_(true) {
    stats_ = {}; // Initialize stats
}

RawCapture::~RawCapture() {
    stopCapture();
}

bool RawCapture::initialize(const CaptureConfig& config) {
    config_ = config;
    sync_enabled_ = config.sync_av;
    
    // Generate filenames
    final_filename_ = config.output_path;
    temp_filename_ = final_filename_ + ".tmp";
    
    // Validate output directory
    std::filesystem::path output_path(final_filename_);
    std::filesystem::path output_dir = output_path.parent_path();
    
    if (!output_dir.empty() && !std::filesystem::exists(output_dir)) {
        std::error_code ec;
        if (!std::filesystem::create_directories(output_dir, ec)) {
            std::cerr << "Failed to create output directory: " << ec.message() << std::endl;
            return false;
        }
    }
    
    std::cout << "RAW capture initialized:" << std::endl;
    std::cout << "  Output: " << final_filename_ << std::endl;
    std::cout << "  Video: " << (config.capture_video ? "Yes" : "No") << std::endl;
    std::cout << "  Audio: " << (config.capture_audio ? "Yes" : "No") << std::endl;
    std::cout << "  A/V Sync: " << (sync_enabled_ ? "Yes" : "No") << std::endl;
    std::cout << "  Mode: " << (config.mode == CaptureConfig::Mode::LIVE ? "Live" : "Quality") << std::endl;
    
    return true;
}

bool RawCapture::startCapture() {
    if (capturing_.load()) {
        std::cerr << "Already capturing!" << std::endl;
        return false;
    }
    
    // Open output file
    output_file_.open(temp_filename_, std::ios::binary);
    if (!output_file_.is_open()) {
        std::cerr << "Failed to open output file: " << temp_filename_ << std::endl;
        return false;
    }
    
    // Write file header
    if (!writeFileHeader()) {
        std::cerr << "Failed to write file header" << std::endl;
        output_file_.close();
        return false;
    }
    
    // Initialize timing
    capture_start_time_ = std::chrono::high_resolution_clock::now();
    stats_.start_time = capture_start_time_;
    
    // Reset stats
    stats_.video_frames_captured = 0;
    stats_.video_frames_dropped = 0;
    stats_.audio_packets_captured = 0;
    stats_.audio_packets_dropped = 0;
    stats_.bytes_written = sizeof(RawFileHeader);
    
    // Start processing thread
    should_stop_ = false;
    capturing_ = true;
    processing_thread_ = std::thread(&RawCapture::processingLoop, this);
    
    std::cout << "RAW capture started: " << temp_filename_ << std::endl;
    return true;
}

void RawCapture::stopCapture() {
    if (!capturing_.load()) return;
    
    std::cout << "Stopping RAW capture..." << std::endl;
    
    // Signal stop
    should_stop_ = true;
    data_available_.notify_all();
    
    // Wait for processing thread
    if (processing_thread_.joinable()) {
        processing_thread_.join();
    }
    
    // Close file
    if (output_file_.is_open()) {
        output_file_.close();
    }
    
    // Finalize capture
    finalizeCapture();
    
    capturing_ = false;
    
    // Print final stats
    auto final_stats = getStats();
    std::cout << "RAW capture completed:" << std::endl;
    std::cout << "  Duration: " << final_stats.duration.count() << "ms" << std::endl;
    std::cout << "  Video frames: " << final_stats.video_frames_captured 
              << " (dropped: " << final_stats.video_frames_dropped << ")" << std::endl;
    std::cout << "  Audio packets: " << final_stats.audio_packets_captured 
              << " (dropped: " << final_stats.audio_packets_dropped << ")" << std::endl;
    std::cout << "  File size: " << (final_stats.bytes_written / 1024 / 1024) << " MB" << std::endl;
    std::cout << "  Output: " << final_filename_ << std::endl;
}

void RawCapture::addVideoFrame(const VideoFrame& frame) {
    if (!capturing_.load() || !config_.capture_video) return;
    
    auto timestamp = std::chrono::high_resolution_clock::now();
    
    // Check if we should drop this frame (quality vs latency)
    if (config_.mode == CaptureConfig::Mode::LIVE && shouldDropFrame(timestamp)) {
        stats_.video_frames_dropped++;
        return;
    }
    
    std::unique_lock<std::mutex> lock(video_mutex_);
    
    // Manage queue size
    if (video_queue_.size() >= config_.max_video_queue_size) {
        if (config_.mode == CaptureConfig::Mode::LIVE) {
            // Drop oldest frame in live mode
            video_queue_.pop();
            stats_.video_frames_dropped++;
        } else {
            // In quality mode, wait for space (this might block!)
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return;
        }
    }
    
    video_queue_.push({frame, timestamp});
    lock.unlock();
    
    data_available_.notify_one();
}

void RawCapture::addAudioPacket(const AudioPacket& packet) {
    if (!capturing_.load() || !config_.capture_audio) return;
    
    auto timestamp = std::chrono::high_resolution_clock::now();
    
    std::unique_lock<std::mutex> lock(audio_mutex_);
    
    // Manage queue size
    if (audio_queue_.size() >= config_.max_audio_queue_size) {
        if (config_.mode == CaptureConfig::Mode::LIVE) {
            // Drop oldest packet in live mode
            audio_queue_.pop();
            stats_.audio_packets_dropped++;
        } else {
            // In quality mode, wait for space
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return;
        }
    }
    
    audio_queue_.push({packet, timestamp});
    lock.unlock();
    
    data_available_.notify_one();
}

bool RawCapture::writeFileHeader() {
    RawFileHeader header = {};
    
    header.video_width = Config::Video::FRAME_WIDTH;
    header.video_height = Config::Video::FRAME_HEIGHT;
    header.video_fps_num = static_cast<uint32_t>(Config::Video::FRAME_RATE);
    header.video_fps_den = 1;
    header.audio_sample_rate = Config::Audio::SAMPLE_RATE;
    header.audio_channels = Config::Audio::CHANNELS;
    header.audio_bits_per_sample = Config::Audio::BITS_PER_SAMPLE;
    header.creation_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    header.flags = 0;
    
    if (sync_enabled_) header.flags |= 0x01;
    if (config_.capture_video) header.flags |= 0x02;
    if (config_.capture_audio) header.flags |= 0x04;
    
    output_file_.write(reinterpret_cast<const char*>(&header), sizeof(header));
    return output_file_.good();
}

void RawCapture::processingLoop() {
    std::cout << "RAW capture processing thread started" << std::endl;
    
    while (!should_stop_.load()) {
        bool processed_something = false;
        
        // Process video frames
        {
            std::lock_guard<std::mutex> lock(video_mutex_);
            if (!video_queue_.empty()) {
                auto frame = video_queue_.front();
                video_queue_.pop();
                processVideoFrame(frame);
                processed_something = true;
            }
        }
        
        // Process audio packets
        {
            std::lock_guard<std::mutex> lock(audio_mutex_);
            if (!audio_queue_.empty()) {
                auto packet = audio_queue_.front();
                audio_queue_.pop();
                processAudioPacket(packet);
                processed_something = true;
            }
        }
        
        if (!processed_something) {
            // Wait for data
            std::unique_lock<std::mutex> lock(video_mutex_);
            data_available_.wait_for(lock, std::chrono::milliseconds(10));
        }
    }
    
    // Process remaining data
    std::cout << "Processing remaining frames..." << std::endl;
    
    {
        std::lock_guard<std::mutex> lock(video_mutex_);
        while (!video_queue_.empty()) {
            auto frame = video_queue_.front();
            video_queue_.pop();
            processVideoFrame(frame);
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(audio_mutex_);
        while (!audio_queue_.empty()) {
            auto packet = audio_queue_.front();
            audio_queue_.pop();
            processAudioPacket(packet);
        }
    }
    
    std::cout << "RAW capture processing thread finished" << std::endl;
}

void RawCapture::processVideoFrame(const TimestampedVideoFrame& frame) {
    RawFrameHeader header = {};
    header.type = 'V';
    header.timestamp_us = getTimestampMicros(frame.timestamp);
    header.data_size = static_cast<uint32_t>(frame.frame.rgb_data.size());
    header.frame_number = frame.frame.frame_num;
    header.sequence = 0;
    
    // Write frame header
    output_file_.write(reinterpret_cast<const char*>(&header), sizeof(header));
    
    // Write frame data
    output_file_.write(reinterpret_cast<const char*>(frame.frame.rgb_data.data()), 
                      frame.frame.rgb_data.size());
    
    stats_.video_frames_captured++;
    stats_.bytes_written += sizeof(header) + frame.frame.rgb_data.size();
}

void RawCapture::processAudioPacket(const TimestampedAudioPacket& packet) {
    RawFrameHeader header = {};
    header.type = 'A';
    header.timestamp_us = getTimestampMicros(packet.timestamp);
    header.data_size = static_cast<uint32_t>(packet.packet.samples.size() * sizeof(int16_t));
    header.frame_number = 0;
    header.sequence = packet.packet.sequence;
    
    // Write packet header
    output_file_.write(reinterpret_cast<const char*>(&header), sizeof(header));
    
    // Write audio data
    output_file_.write(reinterpret_cast<const char*>(packet.packet.samples.data()), 
                      header.data_size);
    
    stats_.audio_packets_captured++;
    stats_.bytes_written += sizeof(header) + header.data_size;
}

uint64_t RawCapture::getTimestampMicros(const std::chrono::high_resolution_clock::time_point& time) const {
    auto duration = time - capture_start_time_;
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

bool RawCapture::shouldDropFrame(const std::chrono::high_resolution_clock::time_point& timestamp) const {
    // In live mode, drop frames that are too old
    auto age = std::chrono::high_resolution_clock::now() - timestamp;
    return age > std::chrono::milliseconds(100); // Drop frames older than 100ms
}

void RawCapture::finalizeCapture() {
    // Move temp file to final location
    std::error_code ec;
    std::filesystem::rename(temp_filename_, final_filename_, ec);
    if (ec) {
        std::cerr << "Failed to finalize capture file: " << ec.message() << std::endl;
    }
}

CaptureStats RawCapture::getStats() const {
    auto stats = stats_; // Copy atomic values
    auto now = std::chrono::high_resolution_clock::now();
    stats.duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - stats.start_time);
    return stats;
}

std::string RawCapture::getStatusString() const {
    auto stats = getStats();
    
    std::ostringstream oss;
    oss << "RAW Capture - ";
    oss << "Duration: " << stats.duration.count() << "ms, ";
    oss << "Video: " << stats.video_frames_captured << " frames";
    if (stats.video_frames_dropped > 0) {
        oss << " (" << stats.video_frames_dropped << " dropped)";
    }
    oss << ", Audio: " << stats.audio_packets_captured << " packets";
    if (stats.audio_packets_dropped > 0) {
        oss << " (" << stats.audio_packets_dropped << " dropped)";
    }
    oss << ", Size: " << (stats.bytes_written / 1024 / 1024) << " MB";
    
    return oss.str();
}

// Static utility methods
bool RawCapture::validateRawFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;
    
    RawFileHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    return file.good() && 
           std::string(header.magic, 8) == std::string("U64RAW10");
}

RawFileHeader RawCapture::readHeader(const std::string& filename) {
    RawFileHeader header = {};
    
    std::ifstream file(filename, std::ios::binary);
    if (file.is_open()) {
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
    }
    
    return header;
}
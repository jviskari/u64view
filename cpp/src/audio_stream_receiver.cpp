#include "audio_stream_receiver.h"
#include <iostream>
#include <cstring>
#include <algorithm>

AudioStreamReceiver::AudioStreamReceiver()
    : StreamReceiver(Config::Network::MULTICAST_GROUP, Config::Network::AUDIO_PORT),
      last_sequence(0), dropped_packets(0), total_packets(0) {
    // Initialize with epoch time
    last_receive_time = std::chrono::high_resolution_clock::time_point{};
}

AudioStreamReceiver::~AudioStreamReceiver() {
    stop();
}

bool AudioStreamReceiver::initialize() {
    if (!setupMulticastSocket()) {
        return false;
    }
    
    std::cout << "Audio receiver initialized for " << Config::Audio::SAMPLE_RATE 
              << "Hz, " << Config::Audio::CHANNELS << " channels" << std::endl;
    
    stats_time = std::chrono::high_resolution_clock::now();
    return true;
}

void AudioStreamReceiver::start() {
    StreamReceiver::start();
    std::cout << "Audio receiver started" << std::endl;
}

void AudioStreamReceiver::stop() {
    StreamReceiver::stop();
    
    // Clear queue
    std::lock_guard<std::mutex> lock(queue_mutex);
    while (!packet_queue.empty()) {
        packet_queue.pop();
    }
}

void AudioStreamReceiver::processPacket(const char* data, size_t size) {
    if (size < 2) return; // Need at least sequence number
    
    total_packets++;
    
    // Update last receive time in thread-safe manner
    setLastReceiveTime(std::chrono::high_resolution_clock::now());
    
    // Parse sequence number (little-endian)
    uint16_t sequence;
    std::memcpy(&sequence, data, sizeof(sequence));
    
    // Check for dropped packets
    if (total_packets > 1) {
        uint16_t expected = (last_sequence.load() + 1) & 0xFFFF;
        if (sequence != expected && sequence != last_sequence.load()) {
            dropped_packets++;
            if (Config::Debug::SHOW_PACKET_STATS) {
                std::cout << "X" << std::flush;
            }
        } else if (Config::Debug::SHOW_PACKET_STATS) {
            std::cout << "." << std::flush;
        }
    }
    last_sequence = sequence;
    
    // Convert audio data to samples
    const char* audio_data = data + 2; // Skip sequence number
    size_t audio_data_size = size - 2;
    size_t sample_count = audio_data_size / sizeof(int16_t);
    
    if (sample_count == 0) return;
    
    AudioPacket packet;
    packet.sequence = sequence;
    packet.receive_time = std::chrono::high_resolution_clock::now();
    packet.samples.resize(sample_count);
    
    // Copy samples directly (assuming they're already in correct format)
    std::memcpy(packet.samples.data(), audio_data, audio_data_size);
    
    // Add to queue with better size management
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        
        // Keep queue size reasonable but not too small
        while (packet_queue.size() >= Config::Audio::MAX_AUDIO_QUEUE_SIZE) {
            packet_queue.pop(); // Remove oldest
            dropped_packets++; // Count as dropped
        }
        
        packet_queue.push(packet);
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

bool AudioStreamReceiver::hasAudioData() {
    std::lock_guard<std::mutex> lock(queue_mutex);
    return !packet_queue.empty();
}

std::vector<int16_t> AudioStreamReceiver::getNextSamples(size_t max_samples) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    
    std::vector<int16_t> result;
    
    // Collect samples from available packets
    while (result.size() < max_samples && !packet_queue.empty()) {
        AudioPacket& packet = packet_queue.front();
        
        size_t samples_needed = max_samples - result.size();
        size_t samples_available = packet.samples.size();
        size_t samples_to_take = std::min(samples_needed, samples_available);
        
        // Copy samples to result
        result.insert(result.end(), 
                     packet.samples.begin(), 
                     packet.samples.begin() + samples_to_take);
        
        // Remove taken samples from packet
        if (samples_to_take >= samples_available) {
            // Consumed entire packet
            packet_queue.pop();
        } else {
            // Partial consumption - remove used samples
            packet.samples.erase(packet.samples.begin(), 
                               packet.samples.begin() + samples_to_take);
        }
    }
    
    return result;
}

size_t AudioStreamReceiver::getQueueSize() const {
    std::lock_guard<std::mutex> lock(queue_mutex);
    return packet_queue.size();
}

std::chrono::high_resolution_clock::time_point AudioStreamReceiver::getLastReceiveTime() const {
    std::lock_guard<std::mutex> lock(time_mutex);
    return last_receive_time;
}

void AudioStreamReceiver::setLastReceiveTime(const std::chrono::high_resolution_clock::time_point& time) {
    std::lock_guard<std::mutex> lock(time_mutex);
    last_receive_time = time;
}

void AudioStreamReceiver::printStats() {
    double drop_rate = total_packets > 0 ? (100.0 * dropped_packets / total_packets) : 0.0;
    std::cout << "\nAudio Stats - Total: " << total_packets.load()
              << ", Dropped: " << dropped_packets.load()
              << " (" << drop_rate << "%), Queue: " << getQueueSize() << std::endl;
}
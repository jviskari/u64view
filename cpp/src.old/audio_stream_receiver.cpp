#include "audio_stream_receiver.h"
#include <iostream>
#include <cstring>
#include <algorithm>

AudioStreamReceiver::AudioStreamReceiver()
    : StreamReceiver(Config::Network::MULTICAST_GROUP, Config::Network::AUDIO_PORT),
      audio_device(0), last_sequence(0), dropped_packets(0), total_packets(0),
      underrun_count(0) {
    last_receive_time.store(std::chrono::high_resolution_clock::time_point{});
}

AudioStreamReceiver::~AudioStreamReceiver() {
    stop();
}

bool AudioStreamReceiver::initialize() {
    if (!setupMulticastSocket()) {
        return false;
    }
    
    // Setup SDL Audio with more conservative settings
    SDL_AudioSpec desired_spec = {};
    desired_spec.freq = Config::Audio::SAMPLE_RATE;
    desired_spec.format = AUDIO_S16LSB; // Ensure little-endian 16-bit
    desired_spec.channels = Config::Audio::CHANNELS;
    desired_spec.samples = Config::Audio::BUFFER_SAMPLES;
    desired_spec.callback = audioCallback;
    desired_spec.userdata = this;
    
    // Allow SDL to change frequency if needed, but not format/channels
    audio_device = SDL_OpenAudioDevice(nullptr, 0, &desired_spec, &audio_spec, 
                                     SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (audio_device == 0) {
        std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Warn if format changed
    if (audio_spec.format != AUDIO_S16LSB) {
        std::cerr << "Warning: Audio format changed from S16LSB to " << audio_spec.format << std::endl;
    }
    if (audio_spec.channels != Config::Audio::CHANNELS) {
        std::cerr << "Warning: Audio channels changed from " << Config::Audio::CHANNELS 
                  << " to " << static_cast<int>(audio_spec.channels) << std::endl;
    }
    
    std::cout << "Audio initialized: " << audio_spec.freq << "Hz, " 
              << static_cast<int>(audio_spec.channels) << " channels, " 
              << audio_spec.samples << " samples buffer" << std::endl;
    
    stats_time = std::chrono::high_resolution_clock::now();
    return true;
}

void AudioStreamReceiver::start() {
    StreamReceiver::start();
    SDL_PauseAudioDevice(audio_device, 0); // Start playback
    std::cout << "Audio playback started" << std::endl;
}

void AudioStreamReceiver::stop() {
    if (audio_device != 0) {
        SDL_PauseAudioDevice(audio_device, 1); // Pause playback
        SDL_CloseAudioDevice(audio_device);
        audio_device = 0;
    }
    
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
    last_receive_time.store(std::chrono::high_resolution_clock::now());
    
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

bool AudioStreamReceiver::getNextSamples(int16_t* buffer, int sample_count) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    
    // Check if we have enough data
    if (packet_queue.empty()) {
        // Fill with silence and count underrun
        std::memset(buffer, 0, sample_count * sizeof(int16_t));
        underrun_count++;
        return false;
    }
    
    int samples_filled = 0;
    
    // Fill buffer from available packets
    while (samples_filled < sample_count && !packet_queue.empty()) {
        AudioPacket& packet = packet_queue.front();
        
        int samples_needed = sample_count - samples_filled;
        int samples_available = static_cast<int>(packet.samples.size());
        int samples_to_copy = std::min(samples_needed, samples_available);
        
        // Copy samples
        std::memcpy(buffer + samples_filled, packet.samples.data(), 
                   samples_to_copy * sizeof(int16_t));
        
        samples_filled += samples_to_copy;
        
        // Remove packet if fully consumed
        if (samples_to_copy >= samples_available) {
            packet_queue.pop();
        } else {
            // Partial consumption - remove used samples from packet
            packet.samples.erase(packet.samples.begin(), 
                               packet.samples.begin() + samples_to_copy);
        }
    }
    
    // Fill remaining with silence if needed
    if (samples_filled < sample_count) {
        std::memset(buffer + samples_filled, 0, 
                   (sample_count - samples_filled) * sizeof(int16_t));
        underrun_count++;
    }
    
    return samples_filled > 0;
}

size_t AudioStreamReceiver::getQueueSize() const {
    std::lock_guard<std::mutex> lock(queue_mutex);
    return packet_queue.size();
}

std::chrono::high_resolution_clock::time_point AudioStreamReceiver::getLastReceiveTime() const {
    return last_receive_time.load();
}

void AudioStreamReceiver::audioCallback(void* userdata, Uint8* stream, int len) {
    AudioStreamReceiver* receiver = static_cast<AudioStreamReceiver*>(userdata);
    int16_t* buffer = reinterpret_cast<int16_t*>(stream);
    int sample_count = len / sizeof(int16_t);
    
    // Clear the buffer first
    std::memset(stream, 0, len);
    
    // Fill with audio data
    receiver->getNextSamples(buffer, sample_count);
}

void AudioStreamReceiver::printStats() {
    double drop_rate = total_packets > 0 ? (100.0 * dropped_packets / total_packets) : 0.0;
    std::cout << "\nAudio Stats - Total: " << total_packets.load()
              << ", Dropped: " << dropped_packets.load()
              << " (" << drop_rate << "%), Queue: " << getQueueSize()
              << ", Underruns: " << underrun_count.load() << std::endl;
}
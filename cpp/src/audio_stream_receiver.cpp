#include "audio_stream_receiver.h"
#include <iostream>
#include <cstring>

AudioStreamReceiver::AudioStreamReceiver()
    : StreamReceiver(Config::Network::MULTICAST_GROUP, Config::Network::AUDIO_PORT),
      audio_device(0), packets_received(0), packets_dropped(0) {
    
    last_receive_time.store(std::chrono::high_resolution_clock::now());
}

AudioStreamReceiver::~AudioStreamReceiver() {
    stop();
}

bool AudioStreamReceiver::initialize() {
    // Setup SDL audio
    SDL_AudioSpec desired_spec = {};
    desired_spec.freq = Config::Audio::SAMPLE_RATE;
    desired_spec.format = AUDIO_S16SYS; // 16-bit signed
    desired_spec.channels = Config::Audio::CHANNELS;
    desired_spec.samples = Config::Audio::BUFFER_SAMPLES;
    desired_spec.callback = audioCallback;
    desired_spec.userdata = this;
    
    audio_device = SDL_OpenAudioDevice(nullptr, 0, &desired_spec, &audio_spec, 0);
    if (audio_device == 0) {
        std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
        return false;
    }
    
    std::cout << "Audio initialized: " << audio_spec.freq << "Hz, " 
              << static_cast<int>(audio_spec.channels) << " channels, "
              << audio_spec.samples << " samples buffer" << std::endl;
    
    // Initialize network
    return setupMulticastSocket();
}

void AudioStreamReceiver::start() {
    StreamReceiver::start();
    SDL_PauseAudioDevice(audio_device, 0); // Start audio playback
    
    std::cout << "Audio stream receiver started on " 
              << Config::Network::MULTICAST_GROUP << ":" 
              << Config::Network::AUDIO_PORT << std::endl;
}

void AudioStreamReceiver::stop() {
    if (audio_device != 0) {
        SDL_PauseAudioDevice(audio_device, 1); // Pause audio
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
    
    // Update receive time
    last_receive_time.store(std::chrono::high_resolution_clock::now());
    packets_received++;
    
    // Parse sequence number (first 2 bytes, little endian)
    uint16_t sequence;
    std::memcpy(&sequence, data, sizeof(sequence));
    
    // Convert audio data (skip sequence number)
    const int16_t* audio_data = reinterpret_cast<const int16_t*>(data + 2);
    size_t sample_count = (size - 2) / sizeof(int16_t);
    
    // Create audio packet
    AudioPacket packet;
    packet.sequence = sequence;
    packet.samples.assign(audio_data, audio_data + sample_count);
    packet.receive_time = std::chrono::high_resolution_clock::now();
    
    // Add to queue
    std::lock_guard<std::mutex> lock(queue_mutex);
    
    // Limit queue size
    if (packet_queue.size() >= Config::Audio::MAX_AUDIO_QUEUE_SIZE) {
        packet_queue.pop(); // Remove oldest
        packets_dropped++;
    }
    
    packet_queue.push(packet);
}

void AudioStreamReceiver::audioCallback(void* userdata, Uint8* stream, int len) {
    AudioStreamReceiver* receiver = static_cast<AudioStreamReceiver*>(userdata);
    receiver->fillAudioBuffer(stream, len);
}

void AudioStreamReceiver::fillAudioBuffer(Uint8* stream, int len) {
    // Clear buffer first
    std::memset(stream, 0, len);
    
    std::lock_guard<std::mutex> lock(queue_mutex);
    
    int16_t* output = reinterpret_cast<int16_t*>(stream);
    int samples_needed = len / sizeof(int16_t);
    int samples_written = 0;
    
    // Fill buffer from queue
    while (samples_written < samples_needed && !packet_queue.empty()) {
        AudioPacket& packet = packet_queue.front();
        
        int samples_available = packet.samples.size();
        int samples_to_copy = std::min(samples_needed - samples_written, samples_available);
        
        // Copy samples
        std::memcpy(output + samples_written, 
                   packet.samples.data(), 
                   samples_to_copy * sizeof(int16_t));
        
        samples_written += samples_to_copy;
        
        // Remove packet if fully consumed
        if (samples_to_copy >= samples_available) {
            packet_queue.pop();
        } else {
            // Partial consumption - remove consumed samples
            packet.samples.erase(packet.samples.begin(), 
                               packet.samples.begin() + samples_to_copy);
        }
    }
    
    // If we couldn't fill the buffer completely, we have an underrun
    // The remaining buffer is already zeroed, so nothing more to do
}
#ifndef AUDIO_STREAM_RECEIVER_H
#define AUDIO_STREAM_RECEIVER_H

#include "stream_receiver.h"
#include "stream_types.h"  // Add this include
#include "config.h"
#include <SDL.h>
#include <queue>
#include <mutex>
#include <atomic>

class AudioStreamReceiver : public StreamReceiver {
private:
    // SDL Audio
    SDL_AudioDeviceID audio_device;
    SDL_AudioSpec audio_spec;
    
    // Audio queue
    std::queue<AudioPacket> packet_queue;  // Use AudioPacket from stream_types.h
    mutable std::mutex queue_mutex;
    
    // Statistics
    std::atomic<std::chrono::high_resolution_clock::time_point> last_receive_time;
    std::atomic<size_t> packets_received;
    std::atomic<size_t> packets_dropped;
    
    // Audio callback
    static void audioCallback(void* userdata, Uint8* stream, int len);
    void fillAudioBuffer(Uint8* stream, int len);
    
public:
    AudioStreamReceiver();
    ~AudioStreamReceiver() override;
    
    bool initialize() override;
    void start() override;
    void stop() override;
    
    // Status
    std::chrono::high_resolution_clock::time_point getLastReceiveTime() const {
        return last_receive_time.load();
    }
    
    size_t getQueueSize() const {
        std::lock_guard<std::mutex> lock(queue_mutex);
        return packet_queue.size();
    }
    
    size_t getPacketsReceived() const { return packets_received.load(); }
    size_t getPacketsDropped() const { return packets_dropped.load(); }
    
    // Add method to check if packets are available
    bool hasPacket() const {
        std::lock_guard<std::mutex> lock(queue_mutex);
        return !packet_queue.empty();
    }
    
    // Add method to get next packet for capture
    AudioPacket getNextPacket() {
        std::lock_guard<std::mutex> lock(queue_mutex);
        if (packet_queue.empty()) {
            return AudioPacket{}; // Return empty packet
        }
        
        AudioPacket packet = packet_queue.front();
        packet_queue.pop();
        return packet;
    }

protected:
    void processPacket(const char* data, size_t size) override;
};

#endif // AUDIO_STREAM_RECEIVER_H
#ifndef AUDIO_STREAM_RECEIVER_H
#define AUDIO_STREAM_RECEIVER_H

#include "stream_receiver.h"
#include "config.h"
#include <SDL.h>
#include <queue>
#include <mutex>
#include <atomic>
#include <chrono>

struct AudioPacket {
    uint16_t sequence;
    std::vector<int16_t> samples;
    std::chrono::high_resolution_clock::time_point receive_time;
};

class AudioStreamReceiver : public StreamReceiver {
private:
    SDL_AudioDeviceID audio_device;
    SDL_AudioSpec audio_spec;
    
    std::queue<AudioPacket> packet_queue;
    mutable std::mutex queue_mutex;
    
    // Statistics
    std::atomic<uint16_t> last_sequence;
    std::atomic<size_t> dropped_packets;
    std::atomic<size_t> total_packets;
    std::atomic<size_t> underrun_count; // Add underrun counter
    std::atomic<std::chrono::high_resolution_clock::time_point> last_receive_time;
    std::chrono::high_resolution_clock::time_point stats_time;
    
public:
    AudioStreamReceiver();
    ~AudioStreamReceiver();
    
    bool initialize() override;
    void start() override;
    void stop() override;
    
    // Audio callback access
    bool getNextSamples(int16_t* buffer, int sample_count);
    
    // Statistics
    size_t getQueueSize() const;
    std::chrono::high_resolution_clock::time_point getLastReceiveTime() const;
    size_t getDroppedPacketCount() const { return dropped_packets.load(); }
    size_t getTotalPacketCount() const { return total_packets.load(); }
    size_t getUnderrunCount() const { return underrun_count.load(); }
    
protected:
    void processPacket(const char* data, size_t size) override;
    
private:
    static void audioCallback(void* userdata, Uint8* stream, int len);
    void printStats();
};

#endif // AUDIO_STREAM_RECEIVER_H
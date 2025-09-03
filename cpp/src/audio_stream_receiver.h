#ifndef AUDIO_STREAM_RECEIVER_H
#define AUDIO_STREAM_RECEIVER_H

#include "stream_receiver.h"
#include "config.h"
#include <queue>
#include <mutex>
#include <atomic>
#include <chrono>
#include <vector>

struct AudioPacket {
    uint16_t sequence;
    std::vector<int16_t> samples;
    std::chrono::high_resolution_clock::time_point receive_time;
};

class AudioStreamReceiver : public StreamReceiver {
private:
    std::queue<AudioPacket> packet_queue;
    mutable std::mutex queue_mutex;
    
    // Statistics
    std::atomic<uint16_t> last_sequence;
    std::atomic<size_t> dropped_packets;
    std::atomic<size_t> total_packets;
    
    // Thread-safe time tracking
    mutable std::mutex time_mutex;
    std::chrono::high_resolution_clock::time_point last_receive_time;
    
    std::chrono::high_resolution_clock::time_point stats_time;
    
public:
    AudioStreamReceiver();
    ~AudioStreamReceiver();
    
    bool initialize() override;
    void start() override;
    void stop() override;
    
    // Audio data access (no SDL here!)
    bool hasAudioData();
    std::vector<int16_t> getNextSamples(size_t max_samples);
    
    // Statistics
    size_t getQueueSize() const;
    std::chrono::high_resolution_clock::time_point getLastReceiveTime() const;
    size_t getDroppedPacketCount() const { return dropped_packets.load(); }
    size_t getTotalPacketCount() const { return total_packets.load(); }
    
protected:
    void processPacket(const char* data, size_t size) override;
    
private:
    void printStats();
    void setLastReceiveTime(const std::chrono::high_resolution_clock::time_point& time);
};

#endif // AUDIO_STREAM_RECEIVER_H
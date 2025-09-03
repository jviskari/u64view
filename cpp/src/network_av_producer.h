#ifndef NETWORK_AV_PRODUCER_H
#define NETWORK_AV_PRODUCER_H

#include "av_stream.h"
#include "video_stream_receiver.h"
#include "audio_stream_receiver.h"
#include "av_synchronizer.h"
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>

class NetworkAVProducer : public AVStreamProducer {
private:
    // Network receivers (from existing codebase)
    std::unique_ptr<VideoStreamReceiver> video_receiver;
    std::unique_ptr<AudioStreamReceiver> audio_receiver;
    std::unique_ptr<AVSynchronizer> av_sync;
    
    // Stream consumers
    std::vector<std::shared_ptr<AVStreamConsumer>> consumers;
    std::mutex consumers_mutex;
    
    // Producer thread
    std::thread producer_thread;
    std::atomic<bool> running;
    
    // Frame conversion
    uint64_t next_video_frame_number;
    uint64_t next_audio_frame_number;
    std::chrono::high_resolution_clock::time_point start_time;
    
    // Statistics
    std::atomic<uint64_t> video_frames_produced;
    std::atomic<uint64_t> audio_frames_produced;
    std::chrono::high_resolution_clock::time_point last_stats_time;
    
public:
    NetworkAVProducer();
    ~NetworkAVProducer();
    
    bool initialize() override;
    void start() override;
    void stop() override;
    void addConsumer(std::shared_ptr<AVStreamConsumer> consumer) override;
    void removeConsumer(std::shared_ptr<AVStreamConsumer> consumer) override;
    
    // Stream info
    bool isLive() const override { return true; }
    std::chrono::microseconds getDuration() const override { return std::chrono::microseconds::zero(); }
    std::chrono::microseconds getPosition() const override;
    
    // Statistics
    uint64_t getVideoFramesProduced() const { return video_frames_produced.load(); }
    uint64_t getAudioFramesProduced() const { return audio_frames_produced.load(); }
    
private:
    void producerLoop();
    VideoAVFrame convertVideoFrame(const VideoFrame& frame);
    AudioAVFrame convertAudioFrame(const std::vector<int16_t>& samples, uint64_t timestamp_us);
    void notifyError(const std::string& error);
    void printStats();
};

#endif // NETWORK_AV_PRODUCER_H
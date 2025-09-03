#ifndef FILE_AV_PRODUCER_H
#define FILE_AV_PRODUCER_H

#include "av_stream.h"
#include "av_frame_serialization.h"
#include <fstream>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>

class FileAVProducer : public AVStreamProducer {
private:
    std::string filename;
    std::ifstream file;
    
    std::vector<std::shared_ptr<AVStreamConsumer>> consumers;
    std::mutex consumers_mutex;
    
    std::thread playback_thread;
    std::atomic<bool> running;
    std::atomic<bool> paused;
    std::atomic<uint64_t> frames_read;
    
    // File position tracking
    std::streampos data_start_pos;  // Position after header
    
public:
    FileAVProducer(const std::string& filename);
    ~FileAVProducer();
    
    bool initialize() override;
    void start() override;
    void stop() override;
    void addConsumer(std::shared_ptr<AVStreamConsumer> consumer) override;
    void removeConsumer(std::shared_ptr<AVStreamConsumer> consumer) override;
    
    // Stream info
    bool isLive() const override { return false; }
    std::chrono::microseconds getDuration() const override { return std::chrono::microseconds::zero(); } // Unknown for now
    std::chrono::microseconds getPosition() const override { return std::chrono::microseconds::zero(); } // TODO
    
    // Playback controls
    void pause() { paused = true; }
    void resume() { paused = false; }
    bool isPaused() const { return paused.load(); }
    uint64_t getFramesRead() const { return frames_read.load(); }
    
private:
    void playbackLoop();
    bool skipHeader(); // Skip the minimal header if present
    void notifyError(const std::string& error);
    void notifyStreamEnd();
};

#endif // FILE_AV_PRODUCER_H
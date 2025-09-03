#ifndef AV_SYNCHRONIZER_H
#define AV_SYNCHRONIZER_H

#include "config.h"
#include <chrono>
#include <atomic>

class AVSynchronizer {
private:
    std::chrono::high_resolution_clock::time_point stream_start_time;
    std::chrono::high_resolution_clock::time_point last_video_time;
    std::chrono::high_resolution_clock::time_point last_audio_time;
    std::chrono::high_resolution_clock::time_point last_frame_display_time;
    
    std::atomic<bool> initialized;
    std::atomic<double> current_drift_ms;
    
    // Frame timing
    uint16_t last_displayed_frame;
    bool has_displayed_frame;
    
public:
    AVSynchronizer();
    
    void initialize();
    void updateVideoTimestamp();
    void updateAudioTimestamp();
    
    // Frame display timing
    bool shouldDisplayFrame();
    void frameDisplayed(uint16_t frame_num);
    
    // Sync status
    double getAVDriftMs() const { return current_drift_ms.load(); }
    bool isInSync() const { return std::abs(getAVDriftMs()) <= Config::Sync::MAX_DRIFT_MS; }
    bool isInitialized() const { return initialized.load(); }
    
    // Get next frame display time
    std::chrono::high_resolution_clock::time_point getNextFrameTime() const;
};

#endif // AV_SYNCHRONIZER_H
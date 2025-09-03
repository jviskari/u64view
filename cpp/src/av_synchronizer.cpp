#include "av_synchronizer.h"
#include <iostream>

AVSynchronizer::AVSynchronizer() 
    : initialized(false), current_drift_ms(0.0), last_displayed_frame(0), has_displayed_frame(false) {
}

void AVSynchronizer::initialize() {
    auto now = std::chrono::high_resolution_clock::now();
    stream_start_time = now;
    last_video_time = now;
    last_audio_time = now;
    last_frame_display_time = now;
    initialized = true;
    
    std::cout << "A/V Synchronizer initialized" << std::endl;
}

void AVSynchronizer::updateVideoTimestamp() {
    last_video_time = std::chrono::high_resolution_clock::now();
    
    if (!initialized.load()) {
        initialize();
    }
}

void AVSynchronizer::updateAudioTimestamp() {
    last_audio_time = std::chrono::high_resolution_clock::now();
    
    if (!initialized.load()) {
        initialize();
    }
    
    // Calculate drift (audio leads video = positive drift)
    auto drift = std::chrono::duration_cast<std::chrono::microseconds>(last_audio_time - last_video_time);
    current_drift_ms = drift.count() / 1000.0;
}

bool AVSynchronizer::shouldDisplayFrame() {
    // Always allow display if not initialized or sync is disabled
    if (!initialized.load()) return true;
    
    auto now = std::chrono::high_resolution_clock::now();
    auto next_frame_time = getNextFrameTime();
    
    // Don't block too long - if we're more than 50ms behind, just display
    auto max_delay = std::chrono::milliseconds(50);
    if (next_frame_time > now + max_delay) {
        return true; // Don't wait too long
    }
    
    return now >= next_frame_time;
}

void AVSynchronizer::frameDisplayed(uint16_t frame_num) {
    last_frame_display_time = std::chrono::high_resolution_clock::now();
    last_displayed_frame = frame_num;
    has_displayed_frame = true;
}

std::chrono::high_resolution_clock::time_point AVSynchronizer::getNextFrameTime() const {
    if (!has_displayed_frame) {
        return std::chrono::high_resolution_clock::now();
    }
    
    // Calculate ideal next frame time based on frame rate
    // Fix: Cast to long long to avoid double conversion error
    auto frame_interval = std::chrono::microseconds(static_cast<long long>(1000000.0 / Config::Video::FRAME_RATE));
    
    // Apply gentle sync correction based on A/V drift (but limit it)
    double drift_ms = current_drift_ms.load();
    if (std::abs(drift_ms) > 5.0 && std::abs(drift_ms) < 100.0) { // Only correct reasonable drift
        auto correction = std::chrono::microseconds(static_cast<long long>(drift_ms * Config::Sync::CORRECTION_FACTOR * 1000));
        frame_interval -= correction; // Negative drift = speed up video, positive = slow down
        
        // Limit correction to prevent wild swings
        auto max_correction = std::chrono::microseconds(5000); // 5ms max
        if (correction > max_correction) correction = max_correction;
        if (correction < -max_correction) correction = -max_correction;
    }
    
    return last_frame_display_time + frame_interval;
}
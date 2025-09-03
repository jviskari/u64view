#ifndef ULTIMATE64_VIEWER_H
#define ULTIMATE64_VIEWER_H

#include "video_stream_receiver.h"
#include "audio_stream_receiver.h"
#include "av_synchronizer.h"
#include <SDL.h>
#include <memory>
#include <atomic>

class Ultimate64Viewer {
private:
    // SDL components
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* video_texture;
    
    // Stream receivers
    std::unique_ptr<VideoStreamReceiver> video_receiver;
    std::unique_ptr<AudioStreamReceiver> audio_receiver;
    std::unique_ptr<AVSynchronizer> av_sync;
    
    // Application state
    std::atomic<bool> running;
    
    // Current frame
    VideoFrame current_frame;
    bool has_current_frame;
    
    // Statistics
    std::chrono::high_resolution_clock::time_point stats_time;
    std::chrono::high_resolution_clock::time_point last_frame_time;
    
public:
    Ultimate64Viewer();
    ~Ultimate64Viewer();
    
    bool initialize();
    bool runOnce(); // Run one iteration, return false to exit
    void cleanup();
    
private:
    void handleEvents();
    void updateDisplay();
    void displayFrame(const VideoFrame& frame);
    void showStats();
};

#endif // ULTIMATE64_VIEWER_H
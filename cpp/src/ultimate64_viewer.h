#ifndef ULTIMATE64_VIEWER_H
#define ULTIMATE64_VIEWER_H

#include <SDL.h>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>

#include "config.h"
#include "audio_stream_receiver.h"
#include "video_stream_receiver.h"
#include "capture_interface.h"  // Add this include
#include "stream_types.h"       // Add this include

#ifdef _WIN32
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#endif

class Ultimate64Viewer {
private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    std::unique_ptr<AudioStreamReceiver> audio_receiver;
    std::unique_ptr<VideoStreamReceiver> video_receiver;  // Add video receiver
    std::atomic<bool> running;
    
    // Current frame for display
    VideoFrame current_frame;
    bool has_current_frame;
    
    // Capture functionality
    std::unique_ptr<CaptureInterface> capture_device_;
    
public:
    Ultimate64Viewer();
    ~Ultimate64Viewer();
    
    bool initialize();
    void run();
    void cleanup();
    
    // Capture methods
    bool startRawCapture(const std::string& filename, 
                        CaptureConfig::Mode mode = CaptureConfig::Mode::QUALITY);
    void stopRawCapture();
    bool isCapturing() const;
    std::string getCaptureStatus() const;
    
private:
    void updateDisplay();
    void displayFrame(const VideoFrame& frame);
};

#endif // ULTIMATE64_VIEWER_H
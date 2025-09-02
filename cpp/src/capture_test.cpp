#include "raw_capture.h"
#include "video_stream_receiver.h"
#include "audio_stream_receiver.h"
#include <iostream>
#include <thread>
#include <signal.h>
#include <SDL.h>  // Add SDL include

std::atomic<bool> should_exit{false};

void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nReceived Ctrl+C, stopping capture..." << std::endl;
        should_exit = true;
    }
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    
    std::string output_file = "test_capture.u64raw";
    if (argc > 1) {
        output_file = argv[1];
    }
    
    std::cout << "Starting Ultimate 64 RAW capture test" << std::endl;
    std::cout << "Output file: " << output_file << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    
    // Initialize SDL first
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL could not initialize audio! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }
    
    // Initialize receivers
    auto video_receiver = std::make_unique<VideoStreamReceiver>();
    auto audio_receiver = std::make_unique<AudioStreamReceiver>();
    
    if (!video_receiver->initialize()) {
        std::cerr << "Failed to initialize video receiver" << std::endl;
        SDL_Quit();
        return 1;
    }
    
    if (!audio_receiver->initialize()) {
        std::cerr << "Failed to initialize audio receiver" << std::endl;
        SDL_Quit();
        return 1;
    }
    
    // Initialize capture
    auto capture = std::make_unique<RawCapture>();
    CaptureConfig config;
    config.output_path = output_file;
    config.mode = CaptureConfig::Mode::QUALITY;
    config.capture_video = true;
    config.capture_audio = true;
    
    if (!capture->initialize(config)) {
        std::cerr << "Failed to initialize capture" << std::endl;
        SDL_Quit();
        return 1;
    }
    
    // Start everything
    video_receiver->start();
    audio_receiver->start();
    
    if (!capture->startCapture()) {
        std::cerr << "Failed to start capture" << std::endl;
        SDL_Quit();
        return 1;
    }
    
    std::cout << "Capture started, waiting for data..." << std::endl;
    
    // Main loop
    auto last_stats_time = std::chrono::high_resolution_clock::now();
    
    while (!should_exit.load()) {
        // Feed video frames to capture
        while (video_receiver->hasFrame()) {
            VideoFrame frame = video_receiver->getNextFrame();
            capture->addVideoFrame(frame);
        }
        
        // Feed audio packets to capture
        while (audio_receiver->hasPacket()) {
            AudioPacket packet = audio_receiver->getNextPacket();
            capture->addAudioPacket(packet);
        }
        
        // Print stats every 5 seconds
        auto now = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_time).count() >= 5) {
            std::cout << capture->getStatusString() << std::endl;
            last_stats_time = now;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Stop capture
    std::cout << "Stopping capture..." << std::endl;
    capture->stopCapture();
    
    // Stop receivers
    video_receiver->stop();
    audio_receiver->stop();
    
    // Cleanup SDL
    SDL_Quit();
    
    std::cout << "Capture test completed!" << std::endl;
    return 0;
}
#include "ultimate64_viewer.h"
#include "config.h"
#include <iostream>
#include <thread>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#endif

Ultimate64Viewer::Ultimate64Viewer() 
    : window(nullptr), renderer(nullptr), video_texture(nullptr),
      running(true), has_current_frame(false) {
}

Ultimate64Viewer::~Ultimate64Viewer() {
    cleanup();
}

bool Ultimate64Viewer::initialize() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return false;
    }
#endif
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Create window
    window = SDL_CreateWindow("Ultimate 64 Viewer",
                            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                            Config::Video::FRAME_WIDTH * Config::Video::DISPLAY_SCALE, 
                            Config::Video::FRAME_HEIGHT * Config::Video::DISPLAY_SCALE,
                            SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Create renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Create video texture
    video_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, 
                                    SDL_TEXTUREACCESS_STREAMING, 
                                    Config::Video::FRAME_WIDTH, Config::Video::FRAME_HEIGHT);
    if (!video_texture) {
        std::cerr << "Video texture could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Initialize stream receivers
    video_receiver = std::make_unique<VideoStreamReceiver>();
    if (!video_receiver->initialize()) {
        std::cerr << "Failed to initialize video receiver" << std::endl;
        return false;
    }
    
    audio_receiver = std::make_unique<AudioStreamReceiver>();
    if (!audio_receiver->initialize()) {
        std::cerr << "Failed to initialize audio receiver" << std::endl;
        return false;
    }
    
    // Initialize synchronizer
    av_sync = std::make_unique<AVSynchronizer>();
    
    stats_time = std::chrono::high_resolution_clock::now();
    last_frame_time = std::chrono::high_resolution_clock::now();
    
    std::cout << "Ultimate 64 Viewer initialized successfully" << std::endl;
    std::cout << "Starting Ultimate 64 Viewer" << std::endl;
    std::cout << "Video: " << Config::Network::MULTICAST_GROUP << ":" << Config::Network::VIDEO_PORT << std::endl;
    std::cout << "Audio: " << Config::Network::MULTICAST_GROUP << ":" << Config::Network::AUDIO_PORT << std::endl;
    std::cout << "Press ESC to exit or Ctrl+C" << std::endl;
    
    // Start receivers
    video_receiver->start();
    audio_receiver->start();
    
    return true;
}

bool Ultimate64Viewer::runOnce() {
    if (!running.load()) {
        return false;
    }
    
    auto now = std::chrono::high_resolution_clock::now();
    
    // Handle events - this is critical for responsiveness
    handleEvents();
    
    // Check if we should exit
    if (!running.load()) {
        return false;
    }
    
    // Update display at consistent intervals
    const auto frame_interval = std::chrono::microseconds(20000); // 50Hz = 20ms
    if (now - last_frame_time >= frame_interval) {
        updateDisplay();
        last_frame_time = now;
    }
    
    // Show stats periodically
    if (Config::Debug::SHOW_SYNC_INFO) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - stats_time);
        if (elapsed.count() >= 5) {
            showStats();
            stats_time = now;
        }
    }
    
    return true;
}

void Ultimate64Viewer::handleEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            std::cout << "\nWindow close requested" << std::endl;
            running = false;
        } else if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_ESCAPE) {
                std::cout << "\nESC pressed - exiting" << std::endl;
                running = false;
            }
        }
    }
}

void Ultimate64Viewer::updateDisplay() {
    // Update sync timestamps (but don't let sync block display)
    auto video_last = video_receiver->getLastReceiveTime();
    auto audio_last = audio_receiver->getLastReceiveTime();
    
    if (video_last > std::chrono::high_resolution_clock::time_point{}) {
        av_sync->updateVideoTimestamp();
    }
    if (audio_last > std::chrono::high_resolution_clock::time_point{}) {
        av_sync->updateAudioTimestamp();
    }
    
    // Get new frame if available - simplified logic to avoid blocking
    if (video_receiver->hasFrame()) {
        VideoFrame new_frame = video_receiver->getNextFrame();
        if (new_frame.is_complete) {
            current_frame = new_frame;
            has_current_frame = true;
            av_sync->frameDisplayed(new_frame.frame_num);
        }
    }
    
    // Always display something
    if (has_current_frame) {
        displayFrame(current_frame);
    } else {
        // Show "waiting for signal" screen
        SDL_SetRenderDrawColor(renderer, 32, 32, 32, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }
}

void Ultimate64Viewer::displayFrame(const VideoFrame& frame) {
    // Update texture with RGB data
    void* pixels;
    int pitch;
    if (SDL_LockTexture(video_texture, nullptr, &pixels, &pitch) == 0) {
        std::memcpy(pixels, frame.rgb_data.data(), frame.rgb_data.size());
        SDL_UnlockTexture(video_texture);
        
        // Render
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, video_texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }
}

void Ultimate64Viewer::showStats() {
    std::cout << "\n=== Ultimate 64 Viewer Stats ===" << std::endl;
    std::cout << "Video - Frames: " << video_receiver->getTotalFrameCount()
              << ", Dropped: " << video_receiver->getDroppedFrameCount()
              << ", Queue: " << video_receiver->getQueueSize() << std::endl;
    std::cout << "Audio - Packets: " << audio_receiver->getTotalPacketCount()
              << ", Dropped: " << audio_receiver->getDroppedPacketCount()
              << ", Queue: " << audio_receiver->getQueueSize() << std::endl;
    
    if (av_sync->isInitialized()) {
        std::cout << "Sync - Drift: " << av_sync->getAVDriftMs() << "ms"
                  << ", In Sync: " << (av_sync->isInSync() ? "YES" : "NO") << std::endl;
    } else {
        std::cout << "Sync - Not initialized yet" << std::endl;
    }
}

void Ultimate64Viewer::cleanup() {
    running = false;
    
    std::cout << "Stopping receivers..." << std::endl;
    
    if (video_receiver) {
        video_receiver->stop();
        video_receiver.reset();
    }
    
    if (audio_receiver) {
        audio_receiver->stop();
        audio_receiver.reset();
    }
    
    av_sync.reset();
    
    if (video_texture) {
        SDL_DestroyTexture(video_texture);
        video_texture = nullptr;
    }
    
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    
    SDL_Quit();
    
#ifdef _WIN32
    WSACleanup();
#endif
    
    std::cout << "Cleanup complete" << std::endl;
}
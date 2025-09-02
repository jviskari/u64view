#include "ultimate64_viewer.h"
#include "raw_capture.h"
#include <iostream>

Ultimate64Viewer::Ultimate64Viewer() 
    : window(nullptr), renderer(nullptr), texture(nullptr), 
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
    
    // Create texture
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, 
                              SDL_TEXTUREACCESS_STREAMING, 
                              Config::Video::FRAME_WIDTH, Config::Video::FRAME_HEIGHT);
    if (!texture) {
        std::cerr << "Texture could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Initialize receivers
    audio_receiver = std::make_unique<AudioStreamReceiver>();
    if (!audio_receiver->initialize()) {
        std::cerr << "Failed to initialize audio receiver" << std::endl;
        return false;
    }
    
    video_receiver = std::make_unique<VideoStreamReceiver>();
    if (!video_receiver->initialize()) {
        std::cerr << "Failed to initialize video receiver" << std::endl;
        return false;
    }
    
    return true;
}

void Ultimate64Viewer::run() {
    std::cout << "Starting Ultimate 64 Viewer" << std::endl;
    std::cout << "Listening for video on " << Config::Network::MULTICAST_GROUP 
              << ":" << Config::Network::VIDEO_PORT << std::endl;
    std::cout << "Listening for audio on " << Config::Network::MULTICAST_GROUP 
              << ":" << Config::Network::AUDIO_PORT << std::endl;
    std::cout << "Press ESC to exit, C to start/stop capture" << std::endl;
    
    // Start receivers
    audio_receiver->start();
    video_receiver->start();
    
    SDL_Event e;
    auto last_stats_time = std::chrono::high_resolution_clock::now();
    
    while (running) {
        // Handle events
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT || 
                (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) {
                running = false;
            } else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_c) {
                // Toggle capture
                if (isCapturing()) {
                    stopRawCapture();
                } else {
                    std::string filename = "capture_" + 
                        std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count()) + 
                        ".u64raw";
                    startRawCapture(filename);
                }
            }
        }
        
        updateDisplay();
        
        // Print periodic stats
        auto now = std::chrono::high_resolution_clock::now();
        auto stats_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_time);
        if (stats_elapsed.count() >= 5) {
            std::cout << "\nVideo frames: " << video_receiver->getFramesCompleted()
                      << ", Audio queue: " << audio_receiver->getQueueSize();
            if (isCapturing()) {
                std::cout << ", " << getCaptureStatus();
            }
            std::cout << std::endl;
            last_stats_time = now;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
}

void Ultimate64Viewer::updateDisplay() {
    // Get latest video frame
    while (video_receiver->hasFrame()) {
        current_frame = video_receiver->getNextFrame();
        has_current_frame = true;
        
        // Feed to capture if active
        if (capture_device_ && capture_device_->isCapturing()) {
            capture_device_->addVideoFrame(current_frame);
        }
    }
    
    // Feed audio to capture if active
    if (capture_device_ && capture_device_->isCapturing()) {
        while (audio_receiver->hasPacket()) {
            AudioPacket packet = audio_receiver->getNextPacket();
            capture_device_->addAudioPacket(packet);
        }
    }
    
    // Display current frame
    if (has_current_frame) {
        displayFrame(current_frame);
    } else {
        // Clear screen if no frame
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }
}

void Ultimate64Viewer::displayFrame(const VideoFrame& frame) {
    // Update texture
    void* pixels;
    int pitch;
    SDL_LockTexture(texture, nullptr, &pixels, &pitch);
    
    // Copy RGB data
    const uint8_t* src = frame.rgb_data.data();
    uint8_t* dst = static_cast<uint8_t*>(pixels);
    
    for (int y = 0; y < Config::Video::FRAME_HEIGHT; y++) {
        memcpy(dst + y * pitch, 
               src + y * Config::Video::FRAME_WIDTH * 3, 
               Config::Video::FRAME_WIDTH * 3);
    }
    
    SDL_UnlockTexture(texture);
    
    // Render
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    
    // Show capture indicator
    if (isCapturing()) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // Red dot
        SDL_Rect dot = {10, 10, 20, 20};
        SDL_RenderFillRect(renderer, &dot);
    }
    
    SDL_RenderPresent(renderer);
}

bool Ultimate64Viewer::startRawCapture(const std::string& filename, CaptureConfig::Mode mode) {
    if (capture_device_ && capture_device_->isCapturing()) {
        std::cerr << "Already capturing!" << std::endl;
        return false;
    }
    
    capture_device_ = std::make_unique<RawCapture>();
    
    CaptureConfig config;
    config.output_path = filename;
    config.mode = mode;
    config.capture_video = true;
    config.capture_audio = true;
    config.sync_av = true;
    
    if (!capture_device_->initialize(config)) {
        capture_device_.reset();
        return false;
    }
    
    if (!capture_device_->startCapture()) {
        capture_device_.reset();
        return false;
    }
    
    std::cout << "Started RAW capture to: " << filename << std::endl;
    return true;
}

void Ultimate64Viewer::stopRawCapture() {
    if (capture_device_) {
        capture_device_->stopCapture();
        capture_device_.reset();
        std::cout << "Stopped RAW capture" << std::endl;
    }
}

bool Ultimate64Viewer::isCapturing() const {
    return capture_device_ && capture_device_->isCapturing();
}

std::string Ultimate64Viewer::getCaptureStatus() const {
    if (capture_device_) {
        return capture_device_->getStatusString();
    }
    return "Not capturing";
}

void Ultimate64Viewer::cleanup() {
    running = false;
    
    if (capture_device_) {
        capture_device_->stopCapture();
        capture_device_.reset();
    }
    
    if (audio_receiver) {
        audio_receiver->stop();
        audio_receiver.reset();
    }
    
    if (video_receiver) {
        video_receiver->stop();
        video_receiver.reset();
    }
    
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
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
}
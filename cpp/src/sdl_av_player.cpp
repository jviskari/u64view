#include "sdl_av_player.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <iomanip>

#ifdef _WIN32
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#endif

SDLAVPlayer::SDLAVPlayer(const std::string& title) 
    : window(nullptr), renderer(nullptr), video_texture(nullptr), audio_device(0),
      current_width(0), current_height(0), initialized(false), playing(false),
      window_title(title), frames_displayed(0), audio_samples_played(0) {
}

SDLAVPlayer::~SDLAVPlayer() {
    cleanup();
}

bool SDLAVPlayer::initialize(int window_width, int window_height) {
    if (initialized.load()) {
        std::cout << "SDLAVPlayer already initialized" << std::endl;
        return true;
    }
    
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    
    // Initialize SDL with audio explicitly
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Check audio subsystem
    if (!SDL_WasInit(SDL_INIT_AUDIO)) {
        std::cerr << "SDL Audio subsystem not initialized!" << std::endl;
        return false;
    }
    
    // Create window
    window = SDL_CreateWindow(window_title.c_str(),
                            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                            window_width, window_height,
                            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Create renderer with vsync
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Verify vsync is enabled
    SDL_RendererInfo info;
    SDL_GetRendererInfo(renderer, &info);
    std::cout << "Renderer: " << info.name << std::endl;
    if (info.flags & SDL_RENDERER_PRESENTVSYNC) {
        std::cout << "VSync: Enabled" << std::endl;
    } else {
        std::cout << "VSync: NOT enabled - expect tearing!" << std::endl;
    }
    
    // Setup audio
    if (!setupAudio()) {
        std::cerr << "Failed to setup audio" << std::endl;
        return false;
    }
    
    initialized = true;
    playing = true;
    
    // Initialize timing
    last_frame_time = std::chrono::high_resolution_clock::now();
    last_stats_time = last_frame_time;
    
    std::cout << "SDL AV Player initialized successfully" << std::endl;
    std::cout << "Window: " << window_width << "x" << window_height << std::endl;
    
    return true;
}

bool SDLAVPlayer::setupAudio() {
    SDL_AudioSpec desired_spec = {};
    desired_spec.freq = 48000;
    desired_spec.format = AUDIO_S16LSB;
    desired_spec.channels = 2;
    desired_spec.samples = 1024;
    desired_spec.callback = audioCallback;
    desired_spec.userdata = this;
    
    audio_device = SDL_OpenAudioDevice(nullptr, 0, &desired_spec, &audio_spec, 
                                     SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (audio_device == 0) {
        std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
        return false;
    }
    
    std::cout << "Audio setup: " << audio_spec.freq << "Hz, " 
              << static_cast<int>(audio_spec.channels) << " channels" << std::endl;
    
    // Start audio playback
    SDL_PauseAudioDevice(audio_device, 0);
    
    return true;
}

bool SDLAVPlayer::processEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            std::cout << "Window close requested" << std::endl;
            return false;
        } else if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_ESCAPE) {
                std::cout << "ESC pressed - exiting" << std::endl;
                return false;
            } else if (e.key.keysym.sym == SDLK_SPACE) {
                // Toggle pause (for future use)
                std::cout << "Space pressed" << std::endl;
            } else if (e.key.keysym.sym == SDLK_d) {
                // Print diagnostics
                printDiagnostics();
            }
        }
    }
    return true;
}

void SDLAVPlayer::updateDisplay() {
    if (!initialized.load()) return;
    
    static auto last_present_time = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    
    // Ensure minimum time between presents to avoid overwhelming the GPU
    const auto min_present_interval = std::chrono::microseconds(19000); // Slightly less than 20ms
    if (now - last_present_time < min_present_interval) {
        return;
    }
    
    // Get the next video frame if available
    VideoAVFrame frame_to_display;
    bool has_frame = false;
    
    {
        std::lock_guard<std::mutex> lock(video_mutex);
        if (!video_queue.empty()) {
            frame_to_display = video_queue.front();
            video_queue.pop();
            has_frame = true;
            current_queue_size = video_queue.size();
        }
    }
    
    if (has_frame) {
        // Check for frame drops (non-sequential frame numbers)
        if (last_displayed_frame_num > 0 && 
            frame_to_display.frame_number != last_displayed_frame_num + 1) {
            frames_dropped += frame_to_display.frame_number - last_displayed_frame_num - 1;
        }
        last_displayed_frame_num = frame_to_display.frame_number;
        
        displayVideoFrame(frame_to_display);
        frames_displayed++;
        
        // Calculate inter-frame timing
        auto frame_delta = now - last_frame_time;
        double ms = std::chrono::duration<double, std::milli>(frame_delta).count();
        frame_intervals.push_back(ms);
        if (frame_intervals.size() > 100) {
            frame_intervals.erase(frame_intervals.begin());
        }
        
        last_frame_time = now;
        last_present_time = now;
    } else {
        // No new frame but still present to maintain vsync
        if (renderer) {
            SDL_RenderPresent(renderer);
            last_present_time = now;
        }
    }
    
    // Print stats periodically
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_time).count() >= 5) {
        printDiagnostics();
        last_stats_time = now;
    }
}

void SDLAVPlayer::displayVideoFrame(const VideoAVFrame& frame) {
    // Create or recreate texture if dimensions changed
    if (current_width.load() != frame.width || current_height.load() != frame.height) {
        if (video_texture) {
            SDL_DestroyTexture(video_texture);
        }
        
        video_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, 
                                        SDL_TEXTUREACCESS_STREAMING, 
                                        frame.width, frame.height);
        if (!video_texture) {
            std::cerr << "Failed to create video texture: " << SDL_GetError() << std::endl;
            return;
        }
        
        current_width = frame.width;
        current_height = frame.height;
        
        std::cout << "Created video texture: " << frame.width << "x" << frame.height << std::endl;
    }
    
    // Update texture with RGB data
    void* pixels;
    int pitch;
    if (SDL_LockTexture(video_texture, nullptr, &pixels, &pitch) == 0) {
        // Copy RGB data
        const size_t expected_size = frame.width * frame.height * 3;
        if (frame.rgb_data.size() >= expected_size) {
            // Copy line by line to handle pitch differences
            const uint8_t* src = frame.rgb_data.data();
            uint8_t* dst = static_cast<uint8_t*>(pixels);
            size_t row_size = frame.width * 3;
            
            for (int y = 0; y < frame.height; y++) {
                std::memcpy(dst, src, row_size);
                src += row_size;
                dst += pitch;
            }
        } else {
            std::cerr << "Warning: Frame data size mismatch. Expected " << expected_size 
                      << ", got " << frame.rgb_data.size() << std::endl;
        }
        SDL_UnlockTexture(video_texture);
        
        // Render
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, video_texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }
}

void SDLAVPlayer::onVideoFrame(const VideoAVFrame& frame) {
    if (!playing.load()) return;
    
    std::lock_guard<std::mutex> lock(video_mutex);
    
    // Increase queue size for better buffering
    const size_t MAX_QUEUE_SIZE = 5; // Increased from 3
    while (video_queue.size() >= MAX_QUEUE_SIZE) {
        video_queue.pop(); // Drop oldest frame
        frames_dropped++;
    }
    
    video_queue.push(frame);
}

void SDLAVPlayer::onAudioFrame(const AudioAVFrame& frame) {
    if (!playing.load()) return;
    
    std::lock_guard<std::mutex> lock(audio_mutex);
    
    // Debug: Print audio frame info occasionally
    static int audio_frame_count = 0;
    if (++audio_frame_count % 100 == 0) {
        std::cout << "Audio frame " << frame.frame_number 
                  << ": " << frame.samples.size() << " samples, "
                  << "queue size: " << audio_queue.size() << std::endl;
    }
    
    // Add samples to audio queue
    for (const auto& sample : frame.samples) {
        audio_queue.push(sample);
    }
    
    // Reduce max queue size to prevent excessive buffering
    const size_t max_samples = 48000; // 0.5 seconds instead of 1 second
    size_t dropped = 0;
    while (audio_queue.size() > max_samples) {
        audio_queue.pop();
        dropped++;
    }
    
    if (dropped > 0) {
        std::cout << "WARNING: Dropped " << dropped << " audio samples, queue was full" << std::endl;
    }
}

void SDLAVPlayer::onStreamEnd() {
    std::cout << "Stream ended - displayed " << frames_displayed.load() 
              << " frames, played " << audio_samples_played.load() << " audio samples" << std::endl;
}

void SDLAVPlayer::onStreamError(const std::string& error) {
    std::cerr << "Stream error in SDL player: " << error << std::endl;
}

void SDLAVPlayer::audioCallback(void* userdata, Uint8* stream, int len) {
    static bool first_call = true;
    if (first_call) {
        std::cout << "Audio callback first call! Len: " << len << " bytes" << std::endl;
        first_call = false;
    }
    
    SDLAVPlayer* player = static_cast<SDLAVPlayer*>(userdata);
    if (!player) {
        std::cerr << "ERROR: Audio callback userdata is null!" << std::endl;
        return;
    }
    
    int16_t* buffer = reinterpret_cast<int16_t*>(stream);
    int sample_count = len / sizeof(int16_t);
    
    // Debug callback
    static int callback_count = 0;
    if (++callback_count % 50 == 0) {  // More frequent logging
        std::cout << "Audio callback #" << callback_count 
                  << ": requesting " << sample_count << " samples" << std::endl;
    }
    
    // Clear buffer first
    std::memset(stream, 0, len);
    
    // Fill with audio data
    player->fillAudioBuffer(buffer, sample_count);
}

void SDLAVPlayer::fillAudioBuffer(int16_t* buffer, int sample_count) {
    std::lock_guard<std::mutex> lock(audio_mutex);
    
    int samples_filled = 0;
    while (samples_filled < sample_count && !audio_queue.empty()) {
        buffer[samples_filled] = audio_queue.front();
        audio_queue.pop();
        samples_filled++;
    }
    
    audio_samples_played += samples_filled;
    
    // Debug: Print if we're running out of samples
    if (samples_filled < sample_count) {
        static int underrun_count = 0;
        if (++underrun_count % 10 == 0) {
            std::cout << "Audio underrun: only filled " << samples_filled 
                      << " of " << sample_count << " samples" << std::endl;
        }
    }
    
    // Remaining samples are already zeroed from memset
}

void SDLAVPlayer::printDiagnostics() {
    std::cout << "\n=== SDL Player Diagnostics ===" << std::endl;
    std::cout << "Frames displayed: " << frames_displayed.load() << std::endl;
    std::cout << "Frames dropped: " << frames_dropped.load() << std::endl;
    std::cout << "Current queue size: " << current_queue_size.load() << std::endl;
    std::cout << "Audio samples played: " << audio_samples_played.load() << std::endl;
    
    // Add audio queue status
    {
        std::lock_guard<std::mutex> lock(audio_mutex);
        std::cout << "Audio queue size: " << audio_queue.size() << " samples" << std::endl;
    }
    
    // Check audio device status
    if (audio_device != 0) {
        SDL_AudioStatus status = SDL_GetAudioDeviceStatus(audio_device);
        std::cout << "Audio device status: ";
        switch (status) {
            case SDL_AUDIO_STOPPED: std::cout << "STOPPED"; break;
            case SDL_AUDIO_PLAYING: std::cout << "PLAYING"; break;
            case SDL_AUDIO_PAUSED: std::cout << "PAUSED"; break;
            default: std::cout << "UNKNOWN"; break;
        }
        std::cout << std::endl;
    }
    
    // Analyze frame intervals
    if (!frame_intervals.empty()) {
        double avg = 0, min_val = 1000, max_val = 0;
        for (double interval : frame_intervals) {
            avg += interval;
            min_val = std::min(min_val, interval);
            max_val = std::max(max_val, interval);
        }
        avg /= frame_intervals.size();
        
        std::cout << "\nFrame timing:" << std::endl;
        std::cout << "  Average: " << std::fixed << std::setprecision(2) << avg << " ms" << std::endl;
        std::cout << "  Min: " << min_val << " ms" << std::endl;
        std::cout << "  Max: " << max_val << " ms" << std::endl;
        std::cout << "  Jitter: " << (max_val - min_val) << " ms" << std::endl;
        
        // Show recent intervals
        std::cout << "\nLast 10 frame intervals:" << std::endl;
        size_t start = frame_intervals.size() > 10 ? frame_intervals.size() - 10 : 0;
        for (size_t i = start; i < frame_intervals.size(); i++) {
            std::cout << "  " << std::fixed << std::setprecision(1) << frame_intervals[i] << " ms";
            if (std::abs(frame_intervals[i] - 20.0) > 5.0) {
                std::cout << " ***";
            }
            std::cout << std::endl;
        }
    }
}

void SDLAVPlayer::cleanup() {
    playing = false;
    
    if (audio_device != 0) {
        SDL_PauseAudioDevice(audio_device, 1);
        SDL_CloseAudioDevice(audio_device);
        audio_device = 0;
    }
    
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
    
    if (initialized.load()) {
        SDL_Quit();
        initialized = false;
    }
    
#ifdef _WIN32
    WSACleanup();
#endif
    
    std::cout << "SDL AV Player cleanup complete" << std::endl;
}
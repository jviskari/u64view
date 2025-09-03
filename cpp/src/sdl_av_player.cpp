#include "sdl_av_player.h"
#include <iostream>
#include <algorithm>
#include <cstring>

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
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
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
    
    // Create renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Setup audio
    if (!setupAudio()) {
        std::cerr << "Failed to setup audio" << std::endl;
        return false;
    }
    
    initialized = true;
    playing = true;
    
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
            }
        }
    }
    return true;
}

void SDLAVPlayer::updateDisplay() {
    if (!initialized.load()) return;
    
    // Get the next video frame if available
    VideoAVFrame frame_to_display;
    bool has_frame = false;
    
    {
        std::lock_guard<std::mutex> lock(video_mutex);
        if (!video_queue.empty()) {
            frame_to_display = video_queue.front();
            video_queue.pop();
            has_frame = true;
        }
    }
    
    if (has_frame) {
        displayVideoFrame(frame_to_display);
        frames_displayed++;
    } else {
        // No new frame - just clear to black
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
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
            std::memcpy(pixels, frame.rgb_data.data(), expected_size);
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
    
    // Keep queue size reasonable
    while (video_queue.size() >= 5) {
        video_queue.pop(); // Drop oldest frame
    }
    
    video_queue.push(frame);
}

void SDLAVPlayer::onAudioFrame(const AudioAVFrame& frame) {
    if (!playing.load()) return;
    
    std::lock_guard<std::mutex> lock(audio_mutex);
    
    // Add samples to audio queue
    for (const auto& sample : frame.samples) {
        audio_queue.push(sample);
    }
    
    // Keep queue size reasonable (about 1 second of audio)
    const size_t max_samples = 48000 * 2; // 1 second stereo at 48kHz
    while (audio_queue.size() > max_samples) {
        audio_queue.pop();
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
    SDLAVPlayer* player = static_cast<SDLAVPlayer*>(userdata);
    int16_t* buffer = reinterpret_cast<int16_t*>(stream);
    int sample_count = len / sizeof(int16_t);
    
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
    
    // Remaining samples are already zeroed from memset
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
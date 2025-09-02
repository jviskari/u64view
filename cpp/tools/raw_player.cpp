#include "raw_player.h"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>

RawPlayer::RawPlayer() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
    }
}

RawPlayer::~RawPlayer() {
    closeFile();
    closeAudio();
    
    if (frame_texture_) {
        SDL_DestroyTexture(frame_texture_);
    }
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
    }
    if (window_) {
        SDL_DestroyWindow(window_);
    }
    
    SDL_Quit();
}

bool RawPlayer::loadFile(const std::string& filename) {
    closeFile();
    
    filename_ = filename;
    file_.open(filename, std::ios::binary);
    if (!file_.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return false;
    }
    
    // Read file header
    file_.read(reinterpret_cast<char*>(&file_header_), sizeof(file_header_));
    if (!file_.good()) {
        std::cerr << "Failed to read file header" << std::endl;
        closeFile();
        return false;
    }
    
    // Validate header
    std::string magic(file_header_.magic, 8);
    if (magic != "U64RAW10") {
        std::cerr << "Invalid file format. Expected U64RAW10, got: " << magic << std::endl;
        closeFile();
        return false;
    }
    
    std::cout << "Loaded raw capture file:" << std::endl;
    std::cout << "  Video: " << file_header_.video_width << "x" << file_header_.video_height << std::endl;
    std::cout << "  FPS: " << file_header_.video_fps_num << "/" << file_header_.video_fps_den << std::endl;
    std::cout << "  Audio: " << file_header_.audio_sample_rate << " Hz, " 
              << file_header_.audio_channels << " channels" << std::endl;
    
    // Build frame index
    if (!buildFrameIndex()) {
        std::cerr << "Failed to build frame index" << std::endl;
        closeFile();
        return false;
    }
    
    std::cout << "  Total frames: " << frame_index_.size() << std::endl;
    
    file_loaded_ = true;
    current_frame_index_ = 0;
    
    // Load first frame
    if (!frame_index_.empty()) {
        loadFrame(0);
    }
    
    return true;
}

void RawPlayer::closeFile() {
    if (file_.is_open()) {
        file_.close();
    }
    
    frame_index_.clear();
    current_frame_data_.clear();
    rgb_buffer_.clear();
    file_loaded_ = false;
    current_frame_index_ = 0;
}

bool RawPlayer::buildFrameIndex() {
    frame_index_.clear();
    
    // Start after header
    file_.seekg(sizeof(RawFileHeader));
    
    while (file_.good()) {
        uint64_t current_offset = file_.tellg();
        
        RawFrameHeader frame_header;
        file_.read(reinterpret_cast<char*>(&frame_header), sizeof(frame_header));
        
        if (!file_.good()) {
            break; // End of file
        }
        
        // Store frame info
        FrameInfo info;
        info.file_offset = current_offset;
        info.timestamp_us = frame_header.timestamp_us;
        info.data_size = frame_header.data_size;
        info.frame_number = frame_header.frame_number;
        info.type = frame_header.type;
        
        frame_index_.push_back(info);
        
        // Skip frame data
        file_.seekg(frame_header.data_size, std::ios::cur);
    }
    
    // Sort by timestamp to ensure proper playback order
    std::sort(frame_index_.begin(), frame_index_.end(), 
              [](const FrameInfo& a, const FrameInfo& b) {
                  return a.timestamp_us < b.timestamp_us;
              });
    
    return !frame_index_.empty();
}

bool RawPlayer::initializeDisplay(const std::string& window_title) {
    // Create window
    window_ = SDL_CreateWindow(
        window_title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        file_header_.video_width * 2, file_header_.video_height * 2, // 2x scale
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    
    if (!window_) {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Create renderer
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        std::cerr << "Failed to create renderer: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Create texture for frame data
    frame_texture_ = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING,
        file_header_.video_width,
        file_header_.video_height
    );
    
    if (!frame_texture_) {
        std::cerr << "Failed to create texture: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Initialize RGB buffer
    rgb_buffer_.resize(file_header_.video_width * file_header_.video_height * 3);
    
    return true;
}

bool RawPlayer::loadFrame(size_t frame_index) {
    if (frame_index >= frame_index_.size()) {
        return false;
    }
    
    const FrameInfo& frame_info = frame_index_[frame_index];
    
    if (frame_info.type == 'V') {
        return loadVideoFrame(frame_info);
    } else if (frame_info.type == 'A') {
        return loadAudioFrame(frame_info);
    }
    
    return false;
}

bool RawPlayer::loadVideoFrame(const FrameInfo& frame_info) {
    // Seek to frame data
    file_.seekg(frame_info.file_offset + sizeof(RawFrameHeader));
    
    // Read frame data
    current_frame_data_.resize(frame_info.data_size);
    file_.read(reinterpret_cast<char*>(current_frame_data_.data()), frame_info.data_size);
    
    if (!file_.good()) {
        return false;
    }
    
    // Convert to RGB
    convertFrameToRGB(current_frame_data_);
    
    // Fix: Find the index of this frame_info in the vector
    auto it = std::find_if(frame_index_.begin(), frame_index_.end(),
                          [&frame_info](const FrameInfo& info) {
                              return info.file_offset == frame_info.file_offset &&
                                     info.timestamp_us == frame_info.timestamp_us;
                          });
    
    if (it != frame_index_.end()) {
        current_frame_index_ = std::distance(frame_index_.begin(), it);
    }
    
    return true;
}

bool RawPlayer::loadAudioFrame(const FrameInfo& frame_info) {
    // Audio playback implementation would go here
    // For now, just skip audio frames
    return true;
}

void RawPlayer::convertFrameToRGB(const std::vector<uint8_t>& frame_data) {
    // Check if this is already RGB data or needs conversion from 4-bit pixels
    if (frame_data.size() == rgb_buffer_.size()) {
        // Already RGB data
        std::copy(frame_data.begin(), frame_data.end(), rgb_buffer_.begin());
    } else {
        // Convert from 4-bit packed pixels
        const size_t expected_packed_size = file_header_.video_width * file_header_.video_height / 2;
        
        if (frame_data.size() == expected_packed_size) {
            // Convert 4-bit pixels to RGB using centralized palette
            for (int y = 0; y < static_cast<int>(file_header_.video_height); y++) {
                for (int x = 0; x < static_cast<int>(file_header_.video_width) / 2; x++) {
                    size_t byte_idx = y * (file_header_.video_width / 2) + x;
                    if (byte_idx >= frame_data.size()) break;
                    
                    uint8_t byte = frame_data[byte_idx];
                    uint8_t pixel1 = byte & 0x0F;
                    uint8_t pixel2 = (byte & 0xF0) >> 4;
                    
                    // Set RGB values for both pixels using centralized palette
                    size_t rgb_idx1 = (y * file_header_.video_width + x * 2) * 3;
                    size_t rgb_idx2 = (y * file_header_.video_width + x * 2 + 1) * 3;
                    
                    if (rgb_idx1 + 2 < rgb_buffer_.size()) {
                        VICIIPalette::getRGBArray(pixel1, &rgb_buffer_[rgb_idx1]);
                    }
                    
                    if (rgb_idx2 + 2 < rgb_buffer_.size()) {
                        VICIIPalette::getRGBArray(pixel2, &rgb_buffer_[rgb_idx2]);
                    }
                }
            }
        }
    }
}

bool RawPlayer::play() {
    if (!file_loaded_) return false;
    
    is_playing_ = true;
    is_paused_ = false;
    
    playback_start_time_ = std::chrono::high_resolution_clock::now();
    if (current_frame_index_ < frame_index_.size()) {
        playback_start_timestamp_ = frame_index_[current_frame_index_].timestamp_us;
    }
    
    std::cout << "Playback started" << std::endl;
    return true;
}

void RawPlayer::pause() {
    is_paused_ = !is_paused_;
    
    if (!is_paused_) {
        // Resume - adjust start time
        playback_start_time_ = std::chrono::high_resolution_clock::now();
        if (current_frame_index_ < frame_index_.size()) {
            playback_start_timestamp_ = frame_index_[current_frame_index_].timestamp_us;
        }
    }
    
    std::cout << (is_paused_ ? "Paused" : "Resumed") << std::endl;
}

void RawPlayer::stop() {
    is_playing_ = false;
    is_paused_ = false;
    current_frame_index_ = 0;
    
    if (!frame_index_.empty()) {
        loadFrame(0);
    }
    
    std::cout << "Playback stopped" << std::endl;
}

void RawPlayer::seekToFrame(size_t frame_number) {
    // Find video frame with matching frame number
    for (size_t i = 0; i < frame_index_.size(); i++) {
        if (frame_index_[i].type == 'V' && frame_index_[i].frame_number == frame_number) {
            current_frame_index_ = i;
            loadFrame(i);
            
            // Adjust playback timing
            if (is_playing_) {
                playback_start_time_ = std::chrono::high_resolution_clock::now();
                playback_start_timestamp_ = frame_index_[i].timestamp_us;
            }
            break;
        }
    }
}

void RawPlayer::seekToTime(double seconds) {
    uint64_t target_timestamp = static_cast<uint64_t>(seconds * 1000000); // Convert to microseconds
    
    // Find closest frame
    size_t best_index = 0;
    uint64_t best_diff = UINT64_MAX;
    
    for (size_t i = 0; i < frame_index_.size(); i++) {
        if (frame_index_[i].type == 'V') {
            uint64_t diff = (frame_index_[i].timestamp_us > target_timestamp) ?
                           frame_index_[i].timestamp_us - target_timestamp :
                           target_timestamp - frame_index_[i].timestamp_us;
            
            if (diff < best_diff) {
                best_diff = diff;
                best_index = i;
            }
        }
    }
    
    current_frame_index_ = best_index;
    loadFrame(best_index);
    
    // Adjust playback timing
    if (is_playing_) {
        playback_start_time_ = std::chrono::high_resolution_clock::now();
        playback_start_timestamp_ = frame_index_[best_index].timestamp_us;
    }
}

bool RawPlayer::shouldDisplayFrame() const {
    if (!is_playing_ || is_paused_ || current_frame_index_ >= frame_index_.size()) {
        return false;
    }
    
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - playback_start_time_);
    
    uint64_t current_playback_time = playback_start_timestamp_ + 
                                    static_cast<uint64_t>(elapsed.count() * playback_speed_);
    
    return frame_index_[current_frame_index_].timestamp_us <= current_playback_time;
}

void RawPlayer::render() {
    if (!renderer_) return;
    
    // Update frame if needed
    if (is_playing_ && !is_paused_) {
        while (shouldDisplayFrame() && current_frame_index_ < frame_index_.size()) {
            // Find next video frame
            size_t next_video_frame = current_frame_index_;
            while (next_video_frame < frame_index_.size() && 
                   frame_index_[next_video_frame].type != 'V') {
                next_video_frame++;
            }
            
            if (next_video_frame < frame_index_.size()) {
                loadFrame(next_video_frame);
                current_frame_index_ = next_video_frame + 1;
            } else {
                // End of playback
                stop();
                break;
            }
        }
    }
    
    // Clear screen
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    
    // Render current frame
    if (!rgb_buffer_.empty()) {
        SDL_UpdateTexture(frame_texture_, nullptr, rgb_buffer_.data(), 
                         file_header_.video_width * 3);
        SDL_RenderCopy(renderer_, frame_texture_, nullptr, nullptr);
    }
    
    // Render UI overlay
    renderUI();
    
    SDL_RenderPresent(renderer_);
}

void RawPlayer::renderUI() {
    // Simple UI overlay - could be enhanced with text rendering
    updateStats();
    
    // Draw progress bar at bottom
    int window_width, window_height;
    SDL_GetWindowSize(window_, &window_width, &window_height);
    
    SDL_Rect progress_bg = {10, window_height - 30, window_width - 20, 20};
    SDL_SetRenderDrawColor(renderer_, 64, 64, 64, 255);
    SDL_RenderFillRect(renderer_, &progress_bg);
    
    if (stats_.duration_seconds > 0) {
        int progress_width = static_cast<int>((stats_.current_time / stats_.duration_seconds) * 
                                            (window_width - 20));
        SDL_Rect progress_fill = {10, window_height - 30, progress_width, 20};
        SDL_SetRenderDrawColor(renderer_, 0, 128, 255, 255);
        SDL_RenderFillRect(renderer_, &progress_fill);
    }
}

bool RawPlayer::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                return false;
                
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_SPACE:
                        if (is_playing_) {
                            pause();
                        } else {
                            play();
                        }
                        break;
                        
                    case SDLK_ESCAPE:
                        return false;
                        
                    case SDLK_LEFT:
                        // Seek backward 5 seconds
                        seekToTime(std::max(0.0, stats_.current_time - 5.0));
                        break;
                        
                    case SDLK_RIGHT:
                        // Seek forward 5 seconds
                        seekToTime(stats_.current_time + 5.0);
                        break;
                        
                    case SDLK_HOME:
                        seekToTime(0.0);
                        break;
                        
                    case SDLK_s:
                        stop();
                        break;
                }
                break;
                
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    // Click on progress bar to seek
                    int window_width, window_height;
                    SDL_GetWindowSize(window_, &window_width, &window_height);
                    
                    if (event.button.y >= window_height - 30 && 
                        event.button.x >= 10 && event.button.x <= window_width - 10) {
                        double seek_ratio = static_cast<double>(event.button.x - 10) / (window_width - 20);
                        seekToTime(seek_ratio * stats_.duration_seconds);
                    }
                }
                break;
        }
    }
    
    return true;
}

PlaybackStats RawPlayer::getStats() const {
    updateStats();
    return stats_;
}

void RawPlayer::updateStats() const {
    stats_.total_video_frames = 0;
    stats_.total_audio_packets = 0;
    
    for (const auto& frame : frame_index_) {
        if (frame.type == 'V') {
            stats_.total_video_frames++;
        } else if (frame.type == 'A') {
            stats_.total_audio_packets++;
        }
    }
    
    if (!frame_index_.empty()) {
        stats_.duration_seconds = frame_index_.back().timestamp_us / 1000000.0;
        
        if (current_frame_index_ < frame_index_.size()) {
            stats_.current_time = frame_index_[current_frame_index_].timestamp_us / 1000000.0;
            stats_.current_frame = frame_index_[current_frame_index_].frame_number;
        }
    }
    
    stats_.is_playing = is_playing_;
    stats_.is_paused = is_paused_;
    
    // Calculate FPS
    if (file_header_.video_fps_den > 0) {
        stats_.fps = static_cast<double>(file_header_.video_fps_num) / file_header_.video_fps_den;
    }
    
    // Status message
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    oss << "Frame " << stats_.current_frame << "/" << stats_.total_video_frames;
    oss << " | " << stats_.current_time << "s / " << stats_.duration_seconds << "s";
    oss << " | " << stats_.fps << " FPS";
    if (is_paused_) oss << " | PAUSED";
    stats_.status_message = oss.str();
}

bool RawPlayer::initializeAudio() {
    // Audio initialization would go here
    // Could use SDL_mixer or direct SDL audio
    return true;
}

void RawPlayer::closeAudio() {
    // Audio cleanup would go here
}
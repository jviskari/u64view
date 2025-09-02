#ifndef RAW_PLAYER_H
#define RAW_PLAYER_H

#include "../src/raw_capture.h"
#include "../src/vic_palette.h"
#include "../src/config.h"
#include <SDL.h>
#include <string>
#include <fstream>
#include <vector>
#include <chrono>
#include <memory>

struct PlaybackStats {
    size_t total_video_frames = 0;
    size_t total_audio_packets = 0;
    size_t current_frame = 0;
    double duration_seconds = 0.0;
    double current_time = 0.0;
    double fps = 0.0;
    bool is_playing = false;
    bool is_paused = false;
    std::string status_message;
};

class RawPlayer {
public:
    RawPlayer();
    ~RawPlayer();
    
    // File operations
    bool loadFile(const std::string& filename);
    void closeFile();
    
    // Playback control
    bool play();
    void pause();
    void stop();
    void seekToFrame(size_t frame_number);
    void seekToTime(double seconds);
    
    // Playback state
    bool isPlaying() const { return is_playing_; }
    bool isPaused() const { return is_paused_; }
    PlaybackStats getStats() const;
    
    // Display
    bool initializeDisplay(const std::string& window_title = "U64 Raw Player");
    void render();
    bool handleEvents(); // Returns false when should quit
    
    // Audio (optional - can be implemented later)
    bool initializeAudio();
    void closeAudio();
    
private:
    // File handling
    std::ifstream file_;
    RawFileHeader file_header_;
    std::string filename_;
    bool file_loaded_ = false;
    
    // Frame data
    struct FrameInfo {
        uint64_t file_offset;
        uint64_t timestamp_us;
        uint32_t data_size;
        uint16_t frame_number;
        char type; // 'V' or 'A'
    };
    
    std::vector<FrameInfo> frame_index_;
    size_t current_frame_index_ = 0;
    
    // Display
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* frame_texture_ = nullptr;
    std::vector<uint8_t> current_frame_data_;
    std::vector<uint8_t> rgb_buffer_;
    
    // Timing
    std::chrono::high_resolution_clock::time_point playback_start_time_;
    std::chrono::high_resolution_clock::time_point last_frame_time_;
    uint64_t playback_start_timestamp_ = 0;
    
    // Playback state
    bool is_playing_ = false;
    bool is_paused_ = false;
    double playback_speed_ = 1.0;
    
    // Statistics
    mutable PlaybackStats stats_;
    
    // Private methods
    bool buildFrameIndex();
    bool loadFrame(size_t frame_index);
    bool loadVideoFrame(const FrameInfo& frame_info);
    bool loadAudioFrame(const FrameInfo& frame_info);
    void convertFrameToRGB(const std::vector<uint8_t>& frame_data);
    void updateStats() const;
    void renderFrame();
    void renderUI();
    
    // Timing helpers
    bool shouldDisplayFrame() const;
    void waitForNextFrame();
};

#endif // RAW_PLAYER_H
#ifndef SDL_AV_PLAYER_H
#define SDL_AV_PLAYER_H

#include "av_stream.h"
#include <SDL.h>
#include <memory>
#include <mutex>
#include <atomic>
#include <queue>

class SDLAVPlayer : public AVStreamConsumer {
private:
    // SDL components
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* video_texture;
    SDL_AudioDeviceID audio_device;
    SDL_AudioSpec audio_spec;
    
    // Current frame dimensions
    std::atomic<uint16_t> current_width;
    std::atomic<uint16_t> current_height;
    
    // Video frame queue for display
    std::queue<VideoAVFrame> video_queue;
    std::mutex video_mutex;
    
    // Audio sample queue
    std::queue<int16_t> audio_queue;
    std::mutex audio_mutex;
    
    // Player state
    std::atomic<bool> initialized;
    std::atomic<bool> playing;
    std::string window_title;
    
    // Statistics
    std::atomic<uint64_t> frames_displayed;
    std::atomic<uint64_t> audio_samples_played;
    
public:
    SDLAVPlayer(const std::string& title = "Ultimate64 Viewer");
    ~SDLAVPlayer();
    
    bool initialize(int window_width = 768, int window_height = 544); // 2x scale default
    void cleanup();
    
    // Display control
    bool processEvents(); // Returns false if should quit
    void updateDisplay();
    
    // AVStreamConsumer interface
    void onVideoFrame(const VideoAVFrame& frame) override;
    void onAudioFrame(const AudioAVFrame& frame) override;
    void onStreamEnd() override;
    void onStreamError(const std::string& error) override;
    
    // Status
    bool isInitialized() const { return initialized.load(); }
    bool isPlaying() const { return playing.load(); }
    uint64_t getFramesDisplayed() const { return frames_displayed.load(); }
    uint64_t getAudioSamplesPlayed() const { return audio_samples_played.load(); }
    
private:
    void displayVideoFrame(const VideoAVFrame& frame);
    bool setupAudio();
    static void audioCallback(void* userdata, Uint8* stream, int len);
    void fillAudioBuffer(int16_t* buffer, int sample_count);
};

#endif // SDL_AV_PLAYER_H
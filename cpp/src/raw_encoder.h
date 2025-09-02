#ifndef RAW_ENCODER_H
#define RAW_ENCODER_H

#include "stream_types.h"
#include <string>
#include <memory>
#include <functional>

struct EncoderConfig {
    enum class Format {
        MP4,
        AVI,
        MOV
    };
    
    enum class Quality {
        FAST,      // Fast encoding, larger files
        BALANCED,  // Good quality/speed balance  
        QUALITY,   // Best quality, slower
        LOSSLESS   // Lossless encoding
    };
    
    std::string input_path;
    std::string output_path;
    Format format = Format::MP4;
    Quality quality = Quality::BALANCED;
    
    // Video settings
    int video_bitrate = 5000;  // kbps
    int video_fps = 50;        // PAL rate
    bool deinterlace = false;
    
    // Audio settings  
    int audio_bitrate = 192;   // kbps
    int audio_sample_rate = 48000;
    
    // Processing
    int threads = 0;           // 0 = auto-detect
    bool gpu_acceleration = true;
};

struct EncoderProgress {
    size_t total_frames = 0;
    size_t processed_frames = 0;
    size_t total_audio_packets = 0;
    size_t processed_audio_packets = 0;
    
    double duration_seconds = 0.0;
    double processed_seconds = 0.0;
    
    size_t input_size_bytes = 0;
    size_t output_size_bytes = 0;
    
    double fps = 0.0;
    double compression_ratio = 0.0;
    
    std::string status_message;
    bool is_complete = false;
    bool has_error = false;
    std::string error_message;
    
    double getProgressPercent() const {
        if (total_frames == 0) return 0.0;
        return (100.0 * processed_frames) / total_frames;
    }
    
    std::string getTimeRemaining() const {
        if (processed_frames == 0 || fps == 0.0) return "Unknown";
        
        size_t remaining_frames = total_frames - processed_frames;
        double remaining_seconds = remaining_frames / fps;
        
        int hours = static_cast<int>(remaining_seconds / 3600);
        int minutes = static_cast<int>((remaining_seconds - hours * 3600) / 60);
        int seconds = static_cast<int>(remaining_seconds) % 60;
        
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hours, minutes, seconds);
        return std::string(buffer);
    }
};

using ProgressCallback = std::function<void(const EncoderProgress&)>;

class RawEncoder {
private:
    EncoderConfig config_;
    ProgressCallback progress_callback_;
    
    // FFmpeg context (will be opaque pointers)
    struct FFmpegContext;
    std::unique_ptr<FFmpegContext> ffmpeg_ctx_;
    
public:
    RawEncoder();
    ~RawEncoder();
    
    // Configuration
    bool initialize(const EncoderConfig& config);
    void setProgressCallback(ProgressCallback callback);
    
    // Encoding
    bool encode();
    bool encodeAsync(); // Returns immediately, encoding in background
    
    // Status
    EncoderProgress getProgress() const;
    bool isEncoding() const;
    void cancel();
    
    // Utility
    static bool validateInputFile(const std::string& path);
    static EncoderProgress analyzeFile(const std::string& path);
    static std::vector<std::string> getSupportedFormats();
    
private:
    bool setupFFmpeg();
    bool processVideoFrame(const VideoFrame& frame);
    bool processAudioPacket(const AudioPacket& packet);
    void updateProgress();
    void cleanup();
};

#endif // RAW_ENCODER_H
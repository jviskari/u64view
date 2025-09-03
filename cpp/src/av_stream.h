#ifndef AV_STREAM_H
#define AV_STREAM_H

#include <memory>
#include <functional>
#include <chrono>
#include <vector>
#include <cstdint>

// Frame type enumeration for serialization
enum class AVFrameType : uint8_t {
    VIDEO = 0,
    AUDIO = 1
};

// Base frame structure
struct AVFrame {
    AVFrameType frame_type;
    uint64_t frame_number;
    uint64_t timestamp_us;
    uint64_t presentation_time_us;
    bool is_complete;
    
    AVFrame(AVFrameType type) : frame_type(type), frame_number(0), 
                               timestamp_us(0), presentation_time_us(0), 
                               is_complete(false) {}
    virtual ~AVFrame() = default;
};

struct VideoAVFrame : public AVFrame {
    std::vector<uint8_t> rgb_data;  // Always RGB24 format
    uint16_t width;     // uint16_t is sufficient (max 65535 pixels)
    uint16_t height;    // uint16_t is sufficient (max 65535 pixels)
    
    VideoAVFrame() : AVFrame(AVFrameType::VIDEO), width(0), height(0) {}
};

struct AudioAVFrame : public AVFrame {
    std::vector<int16_t> samples;   // Always S16LE stereo
    uint32_t sample_rate;   // uint32_t for sample rate
    uint16_t channels;      // uint16_t for channel count
    
    AudioAVFrame() : AVFrame(AVFrameType::AUDIO), sample_rate(0), channels(0) {}
};

// Stream consumer interface
class AVStreamConsumer {
public:
    virtual ~AVStreamConsumer() = default;
    virtual void onVideoFrame(const VideoAVFrame& frame) = 0;
    virtual void onAudioFrame(const AudioAVFrame& frame) = 0;
    virtual void onStreamEnd() = 0;
    virtual void onStreamError(const std::string& error) = 0;
};

// Stream producer interface
class AVStreamProducer {
public:
    virtual ~AVStreamProducer() = default;
    virtual bool initialize() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void addConsumer(std::shared_ptr<AVStreamConsumer> consumer) = 0;
    virtual void removeConsumer(std::shared_ptr<AVStreamConsumer> consumer) = 0;
    
    // Stream info
    virtual bool isLive() const = 0;  // true for network, false for file
    virtual std::chrono::microseconds getDuration() const = 0;  // 0 for live
    virtual std::chrono::microseconds getPosition() const = 0;
};

#endif // AV_STREAM_H
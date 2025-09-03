#include "av_frame_serialization.h"
#include <cstring>

bool AVFrameSerializer::serialize(std::ostream& stream, const VideoAVFrame& frame) {
    // Write frame type first
    uint8_t type = static_cast<uint8_t>(frame.frame_type);
    stream.write(reinterpret_cast<const char*>(&type), sizeof(type));
    
    // Write base AVFrame data
    stream.write(reinterpret_cast<const char*>(&frame.frame_number), sizeof(frame.frame_number));
    stream.write(reinterpret_cast<const char*>(&frame.timestamp_us), sizeof(frame.timestamp_us));
    stream.write(reinterpret_cast<const char*>(&frame.presentation_time_us), sizeof(frame.presentation_time_us));
    
    uint8_t is_complete = frame.is_complete ? 1 : 0;
    stream.write(reinterpret_cast<const char*>(&is_complete), sizeof(is_complete));
    
    // Write VideoAVFrame specific data
    stream.write(reinterpret_cast<const char*>(&frame.width), sizeof(frame.width));
    stream.write(reinterpret_cast<const char*>(&frame.height), sizeof(frame.height));
    
    // Write RGB data
    uint32_t data_size = static_cast<uint32_t>(frame.rgb_data.size());
    stream.write(reinterpret_cast<const char*>(&data_size), sizeof(data_size));
    stream.write(reinterpret_cast<const char*>(frame.rgb_data.data()), data_size);
    
    return stream.good();
}

bool AVFrameSerializer::serialize(std::ostream& stream, const AudioAVFrame& frame) {
    // Write frame type first
    uint8_t type = static_cast<uint8_t>(frame.frame_type);
    stream.write(reinterpret_cast<const char*>(&type), sizeof(type));
    
    // Write base AVFrame data
    stream.write(reinterpret_cast<const char*>(&frame.frame_number), sizeof(frame.frame_number));
    stream.write(reinterpret_cast<const char*>(&frame.timestamp_us), sizeof(frame.timestamp_us));
    stream.write(reinterpret_cast<const char*>(&frame.presentation_time_us), sizeof(frame.presentation_time_us));
    
    uint8_t is_complete = frame.is_complete ? 1 : 0;
    stream.write(reinterpret_cast<const char*>(&is_complete), sizeof(is_complete));
    
    // Write AudioAVFrame specific data
    stream.write(reinterpret_cast<const char*>(&frame.sample_rate), sizeof(frame.sample_rate));
    stream.write(reinterpret_cast<const char*>(&frame.channels), sizeof(frame.channels));
    
    // Write sample data
    uint32_t data_size = static_cast<uint32_t>(frame.samples.size() * sizeof(int16_t));
    stream.write(reinterpret_cast<const char*>(&data_size), sizeof(data_size));
    stream.write(reinterpret_cast<const char*>(frame.samples.data()), data_size);
    
    return stream.good();
}

AVFrameType AVFrameSerializer::peekFrameType(std::istream& stream) {
    auto pos = stream.tellg();
    uint8_t type;
    
    if (!stream.read(reinterpret_cast<char*>(&type), sizeof(type))) {
        // Failed to read - could be EOF
        stream.clear(); // Clear error flags
        stream.seekg(pos); // Reset position
        return static_cast<AVFrameType>(255); // Invalid type to indicate error
    }
    
    stream.seekg(pos); // Reset position
    return static_cast<AVFrameType>(type);
}

std::unique_ptr<VideoAVFrame> AVFrameSerializer::deserializeVideo(std::istream& stream) {
    auto frame = std::make_unique<VideoAVFrame>();
    
    // Read base AVFrame data (frame_type already read)
    if (!stream.read(reinterpret_cast<char*>(&frame->frame_number), sizeof(frame->frame_number))) return nullptr;
    if (!stream.read(reinterpret_cast<char*>(&frame->timestamp_us), sizeof(frame->timestamp_us))) return nullptr;
    if (!stream.read(reinterpret_cast<char*>(&frame->presentation_time_us), sizeof(frame->presentation_time_us))) return nullptr;
    
    uint8_t is_complete;
    if (!stream.read(reinterpret_cast<char*>(&is_complete), sizeof(is_complete))) return nullptr;
    frame->is_complete = (is_complete != 0);
    
    // Read VideoAVFrame specific data
    if (!stream.read(reinterpret_cast<char*>(&frame->width), sizeof(frame->width))) return nullptr;
    if (!stream.read(reinterpret_cast<char*>(&frame->height), sizeof(frame->height))) return nullptr;
    
    // Read RGB data
    uint32_t data_size;
    if (!stream.read(reinterpret_cast<char*>(&data_size), sizeof(data_size))) return nullptr;
    
    // Sanity check data size (prevent huge allocations)
    if (data_size > 100 * 1024 * 1024) { // 100MB limit
        return nullptr;
    }
    
    frame->rgb_data.resize(data_size);
    if (!stream.read(reinterpret_cast<char*>(frame->rgb_data.data()), data_size)) return nullptr;
    
    return frame;
}

std::unique_ptr<AudioAVFrame> AVFrameSerializer::deserializeAudio(std::istream& stream) {
    auto frame = std::make_unique<AudioAVFrame>();
    
    // Read base AVFrame data (frame_type already read)
    if (!stream.read(reinterpret_cast<char*>(&frame->frame_number), sizeof(frame->frame_number))) return nullptr;
    if (!stream.read(reinterpret_cast<char*>(&frame->timestamp_us), sizeof(frame->timestamp_us))) return nullptr;
    if (!stream.read(reinterpret_cast<char*>(&frame->presentation_time_us), sizeof(frame->presentation_time_us))) return nullptr;
    
    uint8_t is_complete;
    if (!stream.read(reinterpret_cast<char*>(&is_complete), sizeof(is_complete))) return nullptr;
    frame->is_complete = (is_complete != 0);
    
    // Read AudioAVFrame specific data
    if (!stream.read(reinterpret_cast<char*>(&frame->sample_rate), sizeof(frame->sample_rate))) return nullptr;
    if (!stream.read(reinterpret_cast<char*>(&frame->channels), sizeof(frame->channels))) return nullptr;
    
    // Read sample data
    uint32_t data_size;
    if (!stream.read(reinterpret_cast<char*>(&data_size), sizeof(data_size))) return nullptr;
    
    // Sanity check data size
    if (data_size > 100 * 1024 * 1024) { // 100MB limit
        return nullptr;
    }
    
    frame->samples.resize(data_size / sizeof(int16_t));
    if (!stream.read(reinterpret_cast<char*>(frame->samples.data()), data_size)) return nullptr;
    
    return frame;
}

std::unique_ptr<AVFrame> AVFrameSerializer::deserialize(std::istream& stream) {
    AVFrameType type = peekFrameType(stream);
    
    // Check for invalid type (EOF or error)
    if (static_cast<uint8_t>(type) == 255) {
        return nullptr;
    }
    
    // Skip the type byte since peek read it
    uint8_t type_byte;
    if (!stream.read(reinterpret_cast<char*>(&type_byte), sizeof(type_byte))) {
        return nullptr;
    }
    
    if (type == AVFrameType::VIDEO) {
        return deserializeVideo(stream);
    } else if (type == AVFrameType::AUDIO) {
        return deserializeAudio(stream);
    }
    
    return nullptr;
}
#ifndef AV_FRAME_SERIALIZATION_H
#define AV_FRAME_SERIALIZATION_H

#include "av_stream.h"
#include <iostream>
#include <fstream>

// Serialization interface for AVFrame objects
class AVFrameSerializer {
public:
    // Serialize AVFrame to binary stream
    static bool serialize(std::ostream& stream, const VideoAVFrame& frame);
    static bool serialize(std::ostream& stream, const AudioAVFrame& frame);
    
    // Deserialize AVFrame from binary stream
    static std::unique_ptr<AVFrame> deserialize(std::istream& stream);
    static std::unique_ptr<VideoAVFrame> deserializeVideo(std::istream& stream);
    static std::unique_ptr<AudioAVFrame> deserializeAudio(std::istream& stream);
    
    // Helper to determine frame type from stream
    static AVFrameType peekFrameType(std::istream& stream);
};

#endif // AV_FRAME_SERIALIZATION_H
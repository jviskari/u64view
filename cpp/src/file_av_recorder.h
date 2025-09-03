#ifndef FILE_AV_RECORDER_H
#define FILE_AV_RECORDER_H

#include "av_stream.h"
#include "av_frame_serialization.h"
#include <fstream>
#include <mutex>
#include <atomic>

class FileAVRecorder : public AVStreamConsumer {
private:
    std::string filename;
    std::ofstream file;
    std::mutex file_mutex;
    std::atomic<bool> recording;
    std::atomic<uint64_t> frame_count;
    
public:
    FileAVRecorder(const std::string& filename);
    ~FileAVRecorder();
    
    bool startRecording();
    void stopRecording();
    bool isRecording() const { return recording.load(); }
    uint64_t getFrameCount() const { return frame_count.load(); }
    
    // AVStreamConsumer interface
    void onVideoFrame(const VideoAVFrame& frame) override;
    void onAudioFrame(const AudioAVFrame& frame) override;
    void onStreamEnd() override;
    void onStreamError(const std::string& error) override;
};

#endif // FILE_AV_RECORDER_H
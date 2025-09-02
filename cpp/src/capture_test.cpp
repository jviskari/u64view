#include "raw_capture.h"
#include "video_stream_receiver.h"
#include "stream_receiver.h"
#include "stream_types.h"
#include <iostream>
#include <thread>
#include <signal.h>
#include <queue>
#include <mutex>

// Simple audio receiver that only captures, doesn't play
class CaptureOnlyAudioReceiver : public StreamReceiver {
private:
    std::queue<AudioPacket> packet_queue;
    mutable std::mutex queue_mutex;
    std::atomic<size_t> packets_received{0};
    std::atomic<std::chrono::high_resolution_clock::time_point> last_receive_time;
    
public:
    CaptureOnlyAudioReceiver() 
        : StreamReceiver(Config::Network::MULTICAST_GROUP, Config::Network::AUDIO_PORT) {
        last_receive_time.store(std::chrono::high_resolution_clock::now());
    }
    
    bool initialize() override {
        return setupMulticastSocket();
    }
    
    void start() override {
        StreamReceiver::start();
        std::cout << "Capture-only audio receiver started on " 
                  << Config::Network::MULTICAST_GROUP << ":" 
                  << Config::Network::AUDIO_PORT << std::endl;
    }
    
    void stop() override {
        StreamReceiver::stop();
        std::lock_guard<std::mutex> lock(queue_mutex);
        while (!packet_queue.empty()) {
            packet_queue.pop();
        }
    }
    
    bool hasPacket() const {
        std::lock_guard<std::mutex> lock(queue_mutex);
        return !packet_queue.empty();
    }
    
    AudioPacket getNextPacket() {
        std::lock_guard<std::mutex> lock(queue_mutex);
        if (packet_queue.empty()) {
            return AudioPacket{};
        }
        
        AudioPacket packet = packet_queue.front();
        packet_queue.pop();
        return packet;
    }
    
    size_t getPacketsReceived() const { return packets_received.load(); }
    
    size_t getQueueSize() const {
        std::lock_guard<std::mutex> lock(queue_mutex);
        return packet_queue.size();
    }

protected:
    void processPacket(const char* data, size_t size) override {
        if (size < 2) return;
        
        last_receive_time.store(std::chrono::high_resolution_clock::now());
        packets_received++;
        
        // Parse sequence number
        uint16_t sequence;
        std::memcpy(&sequence, data, sizeof(sequence));
        
        // Convert audio data
        const int16_t* audio_data = reinterpret_cast<const int16_t*>(data + 2);
        size_t sample_count = (size - 2) / sizeof(int16_t);
        
        // Create packet
        AudioPacket packet;
        packet.sequence = sequence;
        packet.samples.assign(audio_data, audio_data + sample_count);
        packet.receive_time = std::chrono::high_resolution_clock::now();
        
        // Add to queue (limit size)
        std::lock_guard<std::mutex> lock(queue_mutex);
        if (packet_queue.size() >= Config::Audio::MAX_AUDIO_QUEUE_SIZE) {
            packet_queue.pop(); // Remove oldest
        }
        packet_queue.push(packet);
    }
};

std::atomic<bool> should_exit{false};

void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nReceived Ctrl+C, stopping capture..." << std::endl;
        should_exit = true;
    }
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    
    std::string output_file = "test_capture.u64raw";
    if (argc > 1) {
        output_file = argv[1];
    }
    
    std::cout << "Starting Ultimate 64 RAW capture test (capture-only mode)" << std::endl;
    std::cout << "Output file: " << output_file << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    
    // Initialize receivers (no SDL needed!)
    auto video_receiver = std::make_unique<VideoStreamReceiver>();
    auto audio_receiver = std::make_unique<CaptureOnlyAudioReceiver>();
    
    if (!video_receiver->initialize()) {
        std::cerr << "Failed to initialize video receiver" << std::endl;
        return 1;
    }
    
    if (!audio_receiver->initialize()) {
        std::cerr << "Failed to initialize audio receiver" << std::endl;
        return 1;
    }
    
    // Initialize capture
    auto capture = std::make_unique<RawCapture>();
    CaptureConfig config;
    config.output_path = output_file;
    config.mode = CaptureConfig::Mode::QUALITY;
    config.capture_video = true;
    config.capture_audio = true;
    
    if (!capture->initialize(config)) {
        std::cerr << "Failed to initialize capture" << std::endl;
        return 1;
    }
    
    // Start everything
    video_receiver->start();
    audio_receiver->start();
    
    if (!capture->startCapture()) {
        std::cerr << "Failed to start capture" << std::endl;
        return 1;
    }
    
    std::cout << "Capture started, waiting for data..." << std::endl;
    
    // Main loop
    auto last_stats_time = std::chrono::high_resolution_clock::now();
    
    while (!should_exit.load()) {
        // Feed video frames to capture
        while (video_receiver->hasFrame()) {
            VideoFrame frame = video_receiver->getNextFrame();
            capture->addVideoFrame(frame);
        }
        
        // Feed audio packets to capture
        while (audio_receiver->hasPacket()) {
            AudioPacket packet = audio_receiver->getNextPacket();
            capture->addAudioPacket(packet);
        }
        
        // Print stats every 5 seconds
        auto now = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_time).count() >= 5) {
            std::cout << "Status: Video frames=" << video_receiver->getFramesCompleted() 
                      << ", Audio packets=" << audio_receiver->getPacketsReceived()
                      << ", Queue sizes: V=" << video_receiver->getFrameQueueSize()
                      << " A=" << audio_receiver->getQueueSize() << std::endl;
            std::cout << capture->getStatusString() << std::endl;
            last_stats_time = now;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Stop capture
    std::cout << "Stopping capture..." << std::endl;
    capture->stopCapture();
    
    // Stop receivers
    video_receiver->stop();
    audio_receiver->stop();
    
    std::cout << "Capture test completed!" << std::endl;
    return 0;
}
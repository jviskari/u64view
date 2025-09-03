#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <cstring>
#include <atomic>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

#include "config.h"

struct VideoPacketHeader {
    uint16_t seq;    // Sequence number
    uint16_t frm;    // Frame number
    uint16_t lin;    // Line number
    uint16_t width;  // Width
    uint8_t lp;      // Lines per packet
    uint8_t bp;      // Bytes per line
    uint16_t enc;    // Encoding
};

class TestStreamGenerator {
private:
    int video_sock;
    int audio_sock;
    struct sockaddr_in video_addr;
    struct sockaddr_in audio_addr;
    
    std::atomic<bool> running;
    std::thread video_thread;
    std::thread audio_thread;
    
    uint16_t video_sequence;
    uint16_t audio_sequence;
    uint16_t frame_number;
    
    // VIC-II color indices
    static constexpr uint8_t COLOR_BLUE = 6;
    static constexpr uint8_t COLOR_RED = 2;
    
public:
    TestStreamGenerator() : video_sequence(0), audio_sequence(0), frame_number(0), running(false) {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    }
    
    ~TestStreamGenerator() {
        stop();
#ifdef _WIN32
        WSACleanup();
#endif
    }
    
    bool initialize() {
        // Create video socket
        video_sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (video_sock < 0) {
            std::cerr << "Failed to create video socket" << std::endl;
            return false;
        }
        
        // Create audio socket
        audio_sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (audio_sock < 0) {
            std::cerr << "Failed to create audio socket" << std::endl;
            return false;
        }
        
        // Setup video multicast address
        memset(&video_addr, 0, sizeof(video_addr));
        video_addr.sin_family = AF_INET;
        video_addr.sin_port = htons(Config::Network::VIDEO_PORT);
        inet_pton(AF_INET, Config::Network::MULTICAST_GROUP, &video_addr.sin_addr);
        
        // Setup audio multicast address
        memset(&audio_addr, 0, sizeof(audio_addr));
        audio_addr.sin_family = AF_INET;
        audio_addr.sin_port = htons(Config::Network::AUDIO_PORT);
        inet_pton(AF_INET, Config::Network::MULTICAST_GROUP, &audio_addr.sin_addr);
        
        // Set multicast TTL
        int ttl = 1;
        setsockopt(video_sock, IPPROTO_IP, IP_MULTICAST_TTL, (const char*)&ttl, sizeof(ttl));
        setsockopt(audio_sock, IPPROTO_IP, IP_MULTICAST_TTL, (const char*)&ttl, sizeof(ttl));
        
        std::cout << "Test stream generator initialized" << std::endl;
        std::cout << "Sending to " << Config::Network::MULTICAST_GROUP 
                  << " - Video port: " << Config::Network::VIDEO_PORT 
                  << ", Audio port: " << Config::Network::AUDIO_PORT << std::endl;
        
        return true;
    }
    
    void start() {
        running = true;
        video_thread = std::thread(&TestStreamGenerator::videoLoop, this);
        audio_thread = std::thread(&TestStreamGenerator::audioLoop, this);
        std::cout << "Test streams started" << std::endl;
    }
    
    void stop() {
        running = false;
        if (video_thread.joinable()) video_thread.join();
        if (audio_thread.joinable()) audio_thread.join();
        
#ifdef _WIN32
        closesocket(video_sock);
        closesocket(audio_sock);
#else
        close(video_sock);
        close(audio_sock);
#endif
    }
    
private:
    void videoLoop() {
        const int LINES_PER_PACKET = 4;
        const int BYTES_PER_LINE = 192; // 384 pixels / 2 pixels per byte
        const int PACKET_PIXEL_DATA_SIZE = LINES_PER_PACKET * BYTES_PER_LINE;
        
        std::vector<uint8_t> packet_buffer(sizeof(VideoPacketHeader) + PACKET_PIXEL_DATA_SIZE);
        
        auto frame_start = std::chrono::high_resolution_clock::now();
        const auto frame_duration = std::chrono::microseconds(20000); // 50Hz = 20ms
        
        while (running) {
            // Determine color based on frame number (50 frames = 1 second at 50Hz)
            bool is_blue = ((frame_number / 50) % 2) == 0;
            uint8_t color = is_blue ? COLOR_BLUE : COLOR_RED;
            uint8_t pixel_byte = (color << 4) | color; // Two pixels of same color
            
            // Send all packets for this frame
            for (int packet_idx = 0; packet_idx < Config::Video::PACKETS_PER_FRAME; packet_idx++) {
                VideoPacketHeader header;
                header.seq = video_sequence++;
                header.frm = frame_number;
                header.lin = packet_idx * LINES_PER_PACKET;
                header.width = Config::Video::FRAME_WIDTH;
                header.lp = LINES_PER_PACKET;
                header.bp = BYTES_PER_LINE;
                header.enc = 0; // Assuming 0 for standard encoding
                
                // Copy header to packet
                memcpy(packet_buffer.data(), &header, sizeof(header));
                
                // Fill pixel data with solid color
                memset(packet_buffer.data() + sizeof(header), pixel_byte, PACKET_PIXEL_DATA_SIZE);
                
                // Send packet
                sendto(video_sock, (const char*)packet_buffer.data(), packet_buffer.size(), 0,
                       (struct sockaddr*)&video_addr, sizeof(video_addr));
            }
            
            frame_number++;
            
            // Maintain 50Hz timing
            frame_start += frame_duration;
            std::this_thread::sleep_until(frame_start);
        }
    }
    
    void audioLoop() {
        const int SAMPLES_PER_PACKET = 480; // ~10ms at 48kHz
        const int PACKET_SIZE = 2 + (SAMPLES_PER_PACKET * 2 * sizeof(int16_t)); // seq + stereo samples
        
        std::vector<uint8_t> packet_buffer(PACKET_SIZE);
        
        auto packet_start = std::chrono::high_resolution_clock::now();
        const auto packet_duration = std::chrono::microseconds(10000); // 10ms
        
        int sample_counter = 0;
        const int SAMPLES_PER_SECOND = Config::Audio::SAMPLE_RATE;
        
        while (running) {
            // Write sequence number
            memcpy(packet_buffer.data(), &audio_sequence, sizeof(audio_sequence));
            audio_sequence++;
            
            // Generate audio samples
            int16_t* samples = reinterpret_cast<int16_t*>(packet_buffer.data() + 2);
            
            for (int i = 0; i < SAMPLES_PER_PACKET; i++) {
                int16_t sample_value = 0;
                
                // Generate a 1kHz beep for 100ms once per second
                int position_in_second = (sample_counter + i) % SAMPLES_PER_SECOND;
                if (position_in_second < SAMPLES_PER_SECOND / 10) { // First 100ms of each second
                    // Generate 1kHz sine wave
                    double t = (double)position_in_second / SAMPLES_PER_SECOND;
                    sample_value = (int16_t)(10000 * sin(2 * M_PI * 1000 * t));
                }
                
                // Stereo: same value for both channels
                samples[i * 2] = sample_value;     // Left
                samples[i * 2 + 1] = sample_value; // Right
            }
            
            sample_counter += SAMPLES_PER_PACKET;
            
            // Send packet
            sendto(audio_sock, (const char*)packet_buffer.data(), packet_buffer.size(), 0,
                   (struct sockaddr*)&audio_addr, sizeof(audio_addr));
            
            // Maintain timing
            packet_start += packet_duration;
            std::this_thread::sleep_until(packet_start);
        }
    }
};

int main() {
    TestStreamGenerator generator;
    
    if (!generator.initialize()) {
        std::cerr << "Failed to initialize test stream generator" << std::endl;
        return 1;
    }
    
    generator.start();
    
    std::cout << "Test stream generator running. Press Enter to stop..." << std::endl;
    std::cout << "Alternating between BLUE and RED screens every second" << std::endl;
    std::cout << "Audio: 1kHz beep at the start of each second" << std::endl;
    
    std::cin.get();
    
    generator.stop();
    std::cout << "Test stream generator stopped" << std::endl;
    
    return 0;
}
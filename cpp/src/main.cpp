#include <SDL.h>  // Changed from <SDL2/SDL.h>
#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <chrono>
#include <cstring>
#include <atomic>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

// Configuration constants
const char* MULTICAST_GROUP = "239.0.1.64";
const int SERVER_PORT = 11000;
const double FRAME_INTERVAL = 1.0 / 50.0; // PAL 50 Hz
const int PACKETS_PER_FRAME = 68;          // 272 lines / 4 lines per packet
const int JITTER_BUFFER_FRAMES = 2;
const int FRAME_WIDTH = 384;
const int FRAME_HEIGHT = 272;

// VIC-II palette (RGB)
const SDL_Color colors[16] = {
    {0x00, 0x00, 0x00, 255}, {0xEF, 0xEF, 0xEF, 255}, {0x8D, 0x2F, 0x34, 255}, {0x6A, 0xD4, 0xCD, 255},
    {0x98, 0x35, 0xA4, 255}, {0x4C, 0xB4, 0x42, 255}, {0x2C, 0x29, 0xB1, 255}, {0xEF, 0xEF, 0x5D, 255},
    {0x98, 0x4E, 0x20, 255}, {0x5B, 0x38, 0x00, 255}, {0xD1, 0x67, 0x6D, 255}, {0x4A, 0x4A, 0x4A, 255},
    {0x7B, 0x7B, 0x7B, 255}, {0x9F, 0xEF, 0x93, 255}, {0x6D, 0x6A, 0xEF, 255}, {0xB2, 0xB2, 0xB2, 255},
};

struct PacketHeader {
    uint16_t seq;
    uint16_t frm;
    uint16_t lin;
    uint16_t width;
    uint8_t lp;
    uint8_t bp;
    uint16_t enc;
};

struct Frame {
    uint16_t frame_num;
    std::vector<uint8_t> pixels; // RGB format
    
    Frame() : frame_num(0), pixels(FRAME_WIDTH * FRAME_HEIGHT * 3, 0) {}
};

class Ultimate64Viewer {
private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    SOCKET sock;
    std::atomic<bool> running;
    
    std::queue<Frame> frame_queue;
    std::mutex queue_mutex;
    
    Frame last_frame;
    uint16_t last_displayed_frame_num;
    bool has_last_frame;
    
public:
    Ultimate64Viewer() : window(nullptr), renderer(nullptr), texture(nullptr), 
                        sock(INVALID_SOCKET), running(true), last_displayed_frame_num(0), 
                        has_last_frame(false) {}
    
    ~Ultimate64Viewer() {
        cleanup();
    }
    
    bool initialize() {
        // Initialize SDL
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
            return false;
        }
        
        // Create window
        window = SDL_CreateWindow("Ultimate 64 Viewer",
                                SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                FRAME_WIDTH * 2, FRAME_HEIGHT * 2, // 2x scale
                                SDL_WINDOW_SHOWN);
        if (!window) {
            std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
            return false;
        }
        
        // Create renderer
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
            return false;
        }
        
        // Create texture
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, 
                                  SDL_TEXTUREACCESS_STREAMING, FRAME_WIDTH, FRAME_HEIGHT);
        if (!texture) {
            std::cerr << "Texture could not be created! SDL_Error: " << SDL_GetError() << std::endl;
            return false;
        }
        
        // Initialize networking
        if (!initializeNetwork()) {
            return false;
        }
        
        return true;
    }
    
    bool initializeNetwork() {
        // Create socket
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock == INVALID_SOCKET) {
            std::cerr << "Socket creation failed" << std::endl;
            return false;
        }
        
        // Set socket to non-blocking - simplified approach for macOS
        int flags = fcntl(sock, F_GETFL, 0);
        if (flags == -1) {
            std::cerr << "fcntl F_GETFL failed" << std::endl;
            return false;
        }
        if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
            std::cerr << "fcntl F_SETFL failed" << std::endl;
            return false;
        }
        
        // Bind socket
        sockaddr_in server_addr = {};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(SERVER_PORT);
        
        if (bind(sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            std::cerr << "Bind failed: " << strerror(errno) << std::endl;
            return false;
        }
        
        // Join multicast group
        ip_mreq mreq = {};
        mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_GROUP);
        mreq.imr_interface.s_addr = INADDR_ANY;
        
        if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == SOCKET_ERROR) {
            std::cerr << "Failed to join multicast group: " << strerror(errno) << std::endl;
            return false;
        }
        
        return true;
    }
    
    void receiverThread() {
        uint16_t current_frame_num = 0;
        std::vector<std::vector<uint8_t>> packets_collected;
        bool frame_started = false;
        
        while (running) {
            char buffer[1024];
            ssize_t bytes_received = recv(sock, buffer, sizeof(buffer), 0);
            
            if (bytes_received == SOCKET_ERROR) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                std::cerr << "recv failed: " << strerror(errno) << std::endl;
                break;
            }
            
            if (bytes_received < 12) continue;
            
            PacketHeader header;
            memcpy(&header, buffer, sizeof(header));
            
            // Convert from little endian if needed
            // (assuming little endian system for simplicity)
            
            if (!frame_started) {
                current_frame_num = header.frm;
                packets_collected.clear();
                frame_started = true;
            }
            
            if (header.frm != current_frame_num) {
                // Process completed frame
                if (packets_collected.size() == PACKETS_PER_FRAME) {
                    processFrame(current_frame_num, packets_collected);
                }
                
                current_frame_num = header.frm;
                packets_collected.clear();
            }
            
            // Store packet data
            std::vector<uint8_t> pixel_data(buffer + 12, buffer + bytes_received);
            packets_collected.push_back(pixel_data);
        }
    }
    
    void processFrame(uint16_t frame_num, const std::vector<std::vector<uint8_t>>& packets) {
        Frame frame;
        frame.frame_num = frame_num;
        
        int y = 0;
        for (const auto& packet : packets) {
            int i = 0;
            for (int line = 0; line < 4 && y < FRAME_HEIGHT; ++line) {
                for (int x = 0; x < 192 && x < FRAME_WIDTH/2; ++x) {
                    if (i >= packet.size()) break;
                    
                    uint8_t b = packet[i++];
                    uint8_t pixel1 = b & 0xF;
                    uint8_t pixel2 = b >> 4;
                    
                    // Set pixel colors (RGB format)
                    int pos1 = (y * FRAME_WIDTH + x * 2) * 3;
                    int pos2 = (y * FRAME_WIDTH + x * 2 + 1) * 3;
                    
                    if (pos1 + 2 < frame.pixels.size()) {
                        frame.pixels[pos1] = colors[pixel1].r;
                        frame.pixels[pos1 + 1] = colors[pixel1].g;
                        frame.pixels[pos1 + 2] = colors[pixel1].b;
                    }
                    
                    if (pos2 + 2 < frame.pixels.size()) {
                        frame.pixels[pos2] = colors[pixel2].r;
                        frame.pixels[pos2 + 1] = colors[pixel2].g;
                        frame.pixels[pos2 + 2] = colors[pixel2].b;
                    }
                }
                y++;
            }
        }
        
        // Add to queue
        std::lock_guard<std::mutex> lock(queue_mutex);
        if (frame_queue.size() >= 10) {
            frame_queue.pop(); // Remove oldest frame
        }
        frame_queue.push(frame);
    }
    
    void run() {
        // Start receiver thread
        std::thread recv_thread(&Ultimate64Viewer::receiverThread, this);
        
        auto next_display_time = std::chrono::high_resolution_clock::now();
        const std::chrono::nanoseconds frame_duration(static_cast<long long>(FRAME_INTERVAL * 1e9));
        
        SDL_Event e;
        while (running) {
            // Handle events
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT || 
                    (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) {
                    running = false;
                }
            }
            
            auto now = std::chrono::high_resolution_clock::now();
            if (now >= next_display_time) {
                next_display_time += frame_duration;
                
                // Get frame from queue
                std::lock_guard<std::mutex> lock(queue_mutex);
                if (frame_queue.size() >= JITTER_BUFFER_FRAMES) {
                    Frame frame = frame_queue.front();
                    frame_queue.pop();
                    
                    if (!has_last_frame) {
                        last_displayed_frame_num = frame.frame_num;
                        last_frame = frame;
                        has_last_frame = true;
                    } else {
                        uint16_t expected_next = (last_displayed_frame_num + 1) & 0xFFFF;
                        if (frame.frame_num == expected_next) {
                            last_displayed_frame_num = frame.frame_num;
                            last_frame = frame;
                        } else if (frame.frame_num != last_displayed_frame_num) {
                            // Skip ahead if lagging
                            last_displayed_frame_num = frame.frame_num;
                            last_frame = frame;
                        }
                        // else duplicate -> keep last_frame
                    }
                }
                
                // Display frame
                if (has_last_frame) {
                    displayFrame(last_frame);
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        recv_thread.join();
    }
    
    void displayFrame(const Frame& frame) {
        // Update texture
        void* pixels;
        int pitch;
        SDL_LockTexture(texture, nullptr, &pixels, &pitch);
        memcpy(pixels, frame.pixels.data(), frame.pixels.size());
        SDL_UnlockTexture(texture);
        
        // Render
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }
    
    void cleanup() {
        running = false;
        
        if (texture) {
            SDL_DestroyTexture(texture);
            texture = nullptr;
        }
        
        if (renderer) {
            SDL_DestroyRenderer(renderer);
            renderer = nullptr;
        }
        
        if (window) {
            SDL_DestroyWindow(window);
            window = nullptr;
        }
        
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
            sock = INVALID_SOCKET;
        }
        
        SDL_Quit();
    }
};

int main(int argc, char* argv[]) {
    Ultimate64Viewer viewer;
    
    if (!viewer.initialize()) {
        std::cerr << "Failed to initialize viewer" << std::endl;
        return 1;
    }
    
    std::cout << "Ultimate 64 Viewer started. Press ESC to exit." << std::endl;
    viewer.run();
    
    return 0;
}

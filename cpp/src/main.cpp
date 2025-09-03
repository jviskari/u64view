#include "network_av_producer.h"
#include "file_av_producer.h"
#include "file_av_recorder.h"
#include "sdl_av_player.h"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

// Global flag for signal handling
std::atomic<bool> g_shutdown_requested{false};

void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nReceived Ctrl+C, shutting down..." << std::endl;
        g_shutdown_requested = true;
    }
}

void printUsage(const char* program_name) {
    std::cout << "Ultimate 64 Viewer - Live stream viewer and file player" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  " << program_name << " live [--record filename.u64]" << std::endl;
    std::cout << "  " << program_name << " play <filename.u64>" << std::endl;
    std::cout << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  live                 - Receive live Ultimate 64 stream" << std::endl;
    std::cout << "  play <file>         - Play recorded .u64 file" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --record <file>     - Record live stream to file" << std::endl;
    std::cout << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  ESC                 - Exit" << std::endl;
    std::cout << "  Ctrl+C              - Exit" << std::endl;
}

int main(int argc, char* argv[]) {
    // Install signal handler
    std::signal(SIGINT, signalHandler);
    
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string command = argv[1];
    
    if (command == "live") {
        // Live stream mode
        std::string record_filename;
        
        // Check for --record option
        if (argc >= 4 && std::string(argv[2]) == "--record") {
            record_filename = argv[3];
        }
        
        std::cout << "=== Ultimate 64 Viewer - Live Mode ===" << std::endl;
        std::cout << "Receiving live stream from Ultimate 64" << std::endl;
        if (!record_filename.empty()) {
            std::cout << "Recording to: " << record_filename << std::endl;
        }
        std::cout << "Press ESC to exit or Ctrl+C" << std::endl;
        
        // Create network producer
        auto producer = std::make_unique<NetworkAVProducer>();
        if (!producer->initialize()) {
            std::cerr << "Failed to initialize network producer" << std::endl;
            return 1;
        }
        
        // Create SDL player
        auto player = std::make_shared<SDLAVPlayer>("Ultimate 64 Viewer - Live");
        if (!player->initialize()) {
            std::cerr << "Failed to initialize SDL player" << std::endl;
            return 1;
        }
        
        // Create recorder if requested
        std::shared_ptr<FileAVRecorder> recorder;
        if (!record_filename.empty()) {
            recorder = std::make_shared<FileAVRecorder>(record_filename);
            if (!recorder->startRecording()) {
                std::cerr << "Failed to start recording" << std::endl;
                return 1;
            }
        }
        
        // Connect producer to consumers
        producer->addConsumer(player);
        if (recorder) {
            producer->addConsumer(recorder);
        }
        
        // Start production
        producer->start();
        
        // Main event loop
        const auto frame_duration = std::chrono::microseconds(20000); // 20ms = 50Hz PAL
        auto next_frame_time = std::chrono::high_resolution_clock::now();
        
        while (!g_shutdown_requested.load()) {
            auto now = std::chrono::high_resolution_clock::now();
            
            // Process SDL events
            if (!player->processEvents()) {
                break; // User closed window or pressed ESC
            }
            
            // Update display at 50Hz
            if (now >= next_frame_time) {
                player->updateDisplay();
                
                // Schedule next frame
                next_frame_time += frame_duration;
                
                // If we're running way behind (more than 2 frames), reset
                if (next_frame_time < now - frame_duration * 2) {
                    next_frame_time = now;
                }
            }
            
            // Calculate sleep time
            auto sleep_time = next_frame_time - now;
            if (sleep_time > std::chrono::milliseconds(1)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        
        // Cleanup
        producer->stop();
        if (recorder) {
            recorder->stopRecording();
        }
        player->cleanup();
        
    } else if (command == "play") {
        // File playback mode
        if (argc < 3) {
            std::cerr << "Error: No filename specified for play command" << std::endl;
            printUsage(argv[0]);
            return 1;
        }
        
        std::string filename = argv[2];
        
        std::cout << "=== Ultimate 64 Viewer - File Player ===" << std::endl;
        std::cout << "Playing: " << filename << std::endl;
        std::cout << "Press ESC to exit or Ctrl+C" << std::endl;
        
        // Create file producer
        auto producer = std::make_unique<FileAVProducer>(filename);
        if (!producer->initialize()) {
            std::cerr << "Failed to initialize file producer" << std::endl;
            return 1;
        }
        
        // Create SDL player
        auto player = std::make_shared<SDLAVPlayer>("Ultimate 64 Viewer - " + filename);
        if (!player->initialize()) {
            std::cerr << "Failed to initialize SDL player" << std::endl;
            return 1;
        }
        
        // Connect producer to player
        producer->addConsumer(player);
        
        // Start playback
        producer->start();
        
        // Give the system time to stabilize before starting the display loop
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Main event loop for file playback
        const auto frame_duration = std::chrono::microseconds(20000); // 20ms = 50Hz PAL
        auto next_frame_time = std::chrono::high_resolution_clock::now();
        auto last_event_check = next_frame_time;
        
        while (!g_shutdown_requested.load()) {
            auto now = std::chrono::high_resolution_clock::now();
            
            // Process SDL events less frequently to avoid timing disruption
            if (now - last_event_check >= std::chrono::milliseconds(10)) {
                if (!player->processEvents()) {
                    break; // User closed window or pressed ESC
                }
                last_event_check = now;
            }
            
            // Update display at exactly 50Hz
            if (now >= next_frame_time) {
                player->updateDisplay();
                
                // Schedule next frame with no drift
                next_frame_time += frame_duration;
                
                // Only reset if we're way behind (more than 2 frames)
                if (next_frame_time < now - frame_duration * 2) {
                    auto behind = std::chrono::duration_cast<std::chrono::milliseconds>(now - next_frame_time);
                    std::cout << "WARNING: Running " << behind.count() << "ms behind, resetting timing" << std::endl;
                    next_frame_time = now;
                }
            }
            
            // High precision sleep
            now = std::chrono::high_resolution_clock::now();
            if (next_frame_time > now) {
                // Use busy-wait for the last millisecond for precision
                auto sleep_time = next_frame_time - now;
                if (sleep_time > std::chrono::milliseconds(1)) {
                    std::this_thread::sleep_until(next_frame_time - std::chrono::milliseconds(1));
                }
                
                // Busy wait for precise timing
                while (std::chrono::high_resolution_clock::now() < next_frame_time) {
                    // Tight loop for precision
                }
            }
        }
        
        // Cleanup
        producer->stop();
        player->cleanup();
        
    } else {
        std::cerr << "Error: Unknown command '" << command << "'" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    std::cout << "Ultimate 64 Viewer finished" << std::endl;
    return 0;
}
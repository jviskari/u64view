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
        while (!g_shutdown_requested.load()) {
            // Process SDL events
            if (!player->processEvents()) {
                break; // User closed window or pressed ESC
            }
            
            // Update display
            player->updateDisplay();
            
            // Small delay
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60fps
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
        
        // Main event loop
        while (!g_shutdown_requested.load()) {
            // Process SDL events
            if (!player->processEvents()) {
                break; // User closed window or pressed ESC
            }
            
            // Update display
            player->updateDisplay();
            
            // Small delay
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60fps
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
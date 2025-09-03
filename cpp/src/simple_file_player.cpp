#include "file_av_producer.h"
#include "sdl_av_player.h"
#include <iostream>
#include <thread>
#include <chrono>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <filename.u64>" << std::endl;
        return 1;
    }
    
    std::string filename = argv[1];
    
    std::cout << "=== Simple U64 File Player ===" << std::endl;
    std::cout << "Playing: " << filename << std::endl;
    std::cout << "Press ESC to exit" << std::endl;
    
    // Create file producer
    auto producer = std::make_unique<FileAVProducer>(filename);
    if (!producer->initialize()) {
        std::cerr << "Failed to initialize file producer" << std::endl;
        return 1;
    }
    
    // Create SDL player
    auto player = std::make_shared<SDLAVPlayer>("U64 File Player - " + filename);
    if (!player->initialize()) {
        std::cerr << "Failed to initialize SDL player" << std::endl;
        return 1;
    }
    
    // Connect producer to player
    producer->addConsumer(player);
    
    // Start playback
    producer->start();
    
    // Main event loop
    bool running = true;
    while (running) {
        // Process SDL events
        if (!player->processEvents()) {
            running = false;
            break;
        }
        
        // Update display
        player->updateDisplay();
        
        // Small delay to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60fps
    }
    
    // Cleanup
    producer->stop();
    player->cleanup();
    
    std::cout << "Playback finished" << std::endl;
    return 0;
}
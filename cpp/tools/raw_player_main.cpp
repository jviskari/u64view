#include "raw_player.h"
#include <iostream>
#include <string>

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " <raw_file>" << std::endl;
    std::cout << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  SPACE    - Play/Pause" << std::endl;
    std::cout << "  S        - Stop" << std::endl;
    std::cout << "  LEFT     - Seek backward 5 seconds" << std::endl;
    std::cout << "  RIGHT    - Seek forward 5 seconds" << std::endl;
    std::cout << "  HOME     - Seek to beginning" << std::endl;
    std::cout << "  ESC      - Quit" << std::endl;
    std::cout << "  Click progress bar to seek" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string filename = argv[1];
    
    // Validate file
    if (!RawCapture::validateRawFile(filename)) {
        std::cerr << "Invalid or unreadable raw capture file: " << filename << std::endl;
        return 1;
    }
    
    // Create player
    RawPlayer player;
    
    // Load file
    if (!player.loadFile(filename)) {
        std::cerr << "Failed to load file: " << filename << std::endl;
        return 1;
    }
    
    // Initialize display
    if (!player.initializeDisplay("U64 Raw Player - " + filename)) {
        std::cerr << "Failed to initialize display" << std::endl;
        return 1;
    }
    
    // Initialize audio (optional)
    player.initializeAudio();
    
    std::cout << "Raw player initialized. Press SPACE to play, ESC to quit." << std::endl;
    
    // Main loop
    bool running = true;
    while (running) {
        running = player.handleEvents();
        player.render();
        
        // Print stats occasionally
        static int frame_counter = 0;
        if (++frame_counter % 60 == 0) {
            auto stats = player.getStats();
            std::cout << "\r" << stats.status_message << std::flush;
        }
        
        SDL_Delay(16); // ~60 FPS
    }
    
    std::cout << std::endl << "Playback finished." << std::endl;
    return 0;
}
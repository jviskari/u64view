#include "ultimate64_viewer.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>  // Add this for std::this_thread::sleep_for

// Global flag for signal handling
std::atomic<bool> g_shutdown_requested{false};

void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nReceived Ctrl+C, shutting down..." << std::endl;
        g_shutdown_requested = true;
    }
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    // Install signal handler
    std::signal(SIGINT, signalHandler);
    
    Ultimate64Viewer viewer;
    
    if (!viewer.initialize()) {
        std::cerr << "Failed to initialize Ultimate 64 Viewer" << std::endl;
        return 1;
    }
    
    // Check for shutdown signal in the main loop
    while (!g_shutdown_requested.load()) {
        if (!viewer.runOnce()) {
            break; // Normal exit (ESC pressed or window closed)
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60fps
    }
    
    std::cout << "Exiting..." << std::endl;
    return 0;
}

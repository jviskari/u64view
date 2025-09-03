#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <chrono>

namespace Config {
    // Network settings
    namespace Network {
        constexpr const char* MULTICAST_GROUP = "239.0.1.64";
        constexpr int VIDEO_PORT = 11000;
        constexpr int AUDIO_PORT = 11001;
        constexpr int SOCKET_TIMEOUT_MS = 1;
        constexpr size_t MAX_PACKET_SIZE = 1024;
    }
    
    // Video settings
    namespace Video {
        constexpr int FRAME_WIDTH = 384;
        constexpr int FRAME_HEIGHT = 272;
        constexpr double FRAME_RATE = 50.0; // PAL 50 Hz
        constexpr int PACKETS_PER_FRAME = 68; // 272 lines / 4 lines per packet
        constexpr size_t MAX_FRAME_QUEUE_SIZE = 10;
        constexpr int JITTER_BUFFER_FRAMES = 2;
        constexpr int DISPLAY_SCALE = 2; // 2x window scaling
    }
    
    // Audio settings
    namespace Audio {
        constexpr int SAMPLE_RATE = 48000;
        constexpr int CHANNELS = 2; // Stereo
        constexpr int BITS_PER_SAMPLE = 16;
        constexpr int BUFFER_SAMPLES = 1024; // ~21ms at 48kHz
        constexpr size_t MAX_AUDIO_QUEUE_SIZE = 50; // ~200ms buffer
        constexpr int BYTES_PER_SAMPLE = BITS_PER_SAMPLE / 8;
        constexpr int FRAME_SIZE = CHANNELS * BYTES_PER_SAMPLE;
    }
    
    // Synchronization settings
    namespace Sync {
        constexpr double MAX_DRIFT_MS = 40.0; // Max allowed A/V drift
        constexpr double CORRECTION_FACTOR = 0.1; // Gentle sync correction
        constexpr double FRAME_INTERVAL_MS = 1000.0 / Video::FRAME_RATE; // 20ms
    }
    
    // Debug settings
    namespace Debug {
        constexpr bool SHOW_PACKET_STATS = true;
        constexpr bool SHOW_SYNC_INFO = true;
        constexpr int STATS_UPDATE_INTERVAL_MS = 1000;
    }
}

#endif // CONFIG_H
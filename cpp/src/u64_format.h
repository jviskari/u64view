#ifndef U64_FORMAT_H
#define U64_FORMAT_H

#include <cstdint>

// Simplified U64 format - just a stream of serialized AVFrame objects
namespace U64Format {
    // Simple file identification (optional)
    constexpr char MAGIC[8] = {'U', '6', '4', 'S', 'T', 'R', 'M', '\0'};
    constexpr uint32_t VERSION = 1;
    
    // Minimal header (optional)
    struct SimpleHeader {
        char magic[8];      // "U64STRM\0" - just for file type identification
        uint32_t version;   // Format version
        uint32_t reserved;  // Padding for alignment
    };
    
    constexpr size_t HEADER_SIZE = sizeof(SimpleHeader);
}

#endif // U64_FORMAT_H
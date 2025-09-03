#ifndef VIC_PALETTE_H
#define VIC_PALETTE_H

#include <SDL.h>
#include <cstdint>

namespace VICIIPalette {
    // VIC-II color palette (RGB values)
    // Based on the original Commodore 64 VIC-II chip colors
    constexpr SDL_Color colors[16] = {
        {0x00, 0x00, 0x00, 255}, // 0: Black
        {0xEF, 0xEF, 0xEF, 255}, // 1: White
        {0x8D, 0x2F, 0x34, 255}, // 2: Red
        {0x6A, 0xD4, 0xCD, 255}, // 3: Cyan
        {0x98, 0x35, 0xA4, 255}, // 4: Purple
        {0x4C, 0xB4, 0x42, 255}, // 5: Green
        {0x2C, 0x29, 0xB1, 255}, // 6: Blue
        {0xEF, 0xEF, 0x5D, 255}, // 7: Yellow
        {0x98, 0x4E, 0x20, 255}, // 8: Orange
        {0x5B, 0x38, 0x00, 255}, // 9: Brown
        {0xD1, 0x67, 0x6D, 255}, // 10: Light Red
        {0x4A, 0x4A, 0x4A, 255}, // 11: Dark Grey
        {0x7B, 0x7B, 0x7B, 255}, // 12: Grey
        {0x9F, 0xEF, 0x93, 255}, // 13: Light Green
        {0x6D, 0x6A, 0xEF, 255}, // 14: Light Blue
        {0xB2, 0xB2, 0xB2, 255}, // 15: Light Grey
    };
    
    // Safe color getter with bounds checking
    inline const SDL_Color& getColor(uint8_t index) {
        return colors[index & 0x0F]; // Mask to ensure valid index (0-15)
    }
    
    // Get RGB components separately (useful for different rendering backends)
    struct RGB {
        uint8_t r, g, b;
    };
    
    inline RGB getRGB(uint8_t index) {
        const SDL_Color& color = getColor(index);
        return {color.r, color.g, color.b};
    }
    
    // Convert 4-bit pixel pair to two RGB colors
    inline void unpackPixelPair(uint8_t byte, RGB& pixel1, RGB& pixel2) {
        pixel1 = getRGB(byte & 0x0F);        // Lower 4 bits
        pixel2 = getRGB((byte & 0xF0) >> 4); // Upper 4 bits
    }
}

#endif // VIC_PALETTE_H
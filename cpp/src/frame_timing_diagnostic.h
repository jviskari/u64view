#ifndef FRAME_TIMING_DIAGNOSTIC_H
#define FRAME_TIMING_DIAGNOSTIC_H

#include <chrono>
#include <vector>
#include <string>

class FrameTimingDiagnostic {
private:
    struct FrameEvent {
        uint64_t frame_number;
        std::chrono::high_resolution_clock::time_point timestamp;
        std::string event_type; // "produced", "consumed", "displayed"
    };
    
    std::vector<FrameEvent> events;
    std::chrono::high_resolution_clock::time_point start_time;
    
public:
    FrameTimingDiagnostic();
    void recordEvent(uint64_t frame_number, const std::string& event_type);
    void analyzeTimings();
};

// Global instance declaration
extern FrameTimingDiagnostic g_timing_diagnostic;

#endif // FRAME_TIMING_DIAGNOSTIC_H
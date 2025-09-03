#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <cmath>

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
    FrameTimingDiagnostic() {
        start_time = std::chrono::high_resolution_clock::now();
        events.reserve(1000);
    }
    
    void recordEvent(uint64_t frame_number, const std::string& event_type) {
        events.push_back({frame_number, std::chrono::high_resolution_clock::now(), event_type});
        
        // Analyze every 100 events
        if (events.size() % 100 == 0) {
            analyzeTimings();
        }
    }
    
    void analyzeTimings() {
        std::cout << "\n=== Frame Timing Analysis ===" << std::endl;
        
        // Group events by type
        std::vector<FrameEvent> produced_events;
        std::vector<FrameEvent> displayed_events;
        
        for (const auto& event : events) {
            if (event.event_type == "produced") {
                produced_events.push_back(event);
            } else if (event.event_type == "displayed") {
                displayed_events.push_back(event);
            }
        }
        
        // Analyze inter-frame timings for produced frames
        if (produced_events.size() > 1) {
            std::vector<double> intervals;
            for (size_t i = 1; i < produced_events.size(); i++) {
                auto delta = produced_events[i].timestamp - produced_events[i-1].timestamp;
                intervals.push_back(std::chrono::duration<double, std::milli>(delta).count());
            }
            
            double avg = 0, min_val = 1000, max_val = 0;
            for (double interval : intervals) {
                avg += interval;
                min_val = std::min(min_val, interval);
                max_val = std::max(max_val, interval);
            }
            avg /= intervals.size();
            
            // Calculate standard deviation
            double variance = 0;
            for (double interval : intervals) {
                variance += (interval - avg) * (interval - avg);
            }
            variance /= intervals.size();
            double std_dev = std::sqrt(variance);
            
            std::cout << "Produced frames timing:" << std::endl;
            std::cout << "  Average: " << std::fixed << std::setprecision(2) << avg << " ms (target: 20.0 ms)" << std::endl;
            std::cout << "  Min: " << min_val << " ms" << std::endl;
            std::cout << "  Max: " << max_val << " ms" << std::endl;
            std::cout << "  Std Dev: " << std_dev << " ms" << std::endl;
            std::cout << "  Jitter: " << (max_val - min_val) << " ms" << std::endl;
        }
        
        // Show recent timing pattern
        std::cout << "\nLast 10 frame intervals (ms):" << std::endl;
        size_t start = produced_events.size() > 10 ? produced_events.size() - 10 : 0;
        for (size_t i = start + 1; i < produced_events.size(); i++) {
            auto delta = produced_events[i].timestamp - produced_events[i-1].timestamp;
            double ms = std::chrono::duration<double, std::milli>(delta).count();
            std::cout << "  Frame " << produced_events[i].frame_number << ": " 
                      << std::fixed << std::setprecision(1) << ms << " ms";
            if (std::abs(ms - 20.0) > 2.0) {
                std::cout << " ***"; // Flag significant deviations
            }
            std::cout << std::endl;
        }
    }
};

// Global instance for easy access
static FrameTimingDiagnostic g_timing_diagnostic;
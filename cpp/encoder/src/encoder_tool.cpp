#include "raw_encoder.h"
#include <iostream>
#include <iomanip>
#include <signal.h>
#include <thread>

std::atomic<bool> should_cancel{false};

void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nReceived Ctrl+C, canceling encoding..." << std::endl;
        should_cancel = true;
    }
}

void printUsage(const char* program_name) {
    std::cout << "Ultimate 64 Raw Encoder\n";
    std::cout << "Usage: " << program_name << " [options] input.u64raw output.mp4\n\n";
    std::cout << "Options:\n";
    std::cout << "  -f, --format FORMAT     Output format (mp4, avi, mov) [default: mp4]\n";
    std::cout << "  -q, --quality QUALITY   Quality preset (fast, balanced, quality, lossless) [default: balanced]\n";
    std::cout << "  -vb, --video-bitrate N  Video bitrate in kbps [default: 5000]\n";
    std::cout << "  -ab, --audio-bitrate N  Audio bitrate in kbps [default: 192]\n";
    std::cout << "  -fps, --framerate N     Output framerate [default: 50]\n";
    std::cout << "  -t, --threads N         Number of threads (0=auto) [default: 0]\n";
    std::cout << "  --no-gpu               Disable GPU acceleration\n";
    std::cout << "  --deinterlace          Apply deinterlacing filter\n";
    std::cout << "  -h, --help             Show this help\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " capture.u64raw video.mp4\n";
    std::cout << "  " << program_name << " -q quality -vb 8000 capture.u64raw hq_video.mp4\n";
    std::cout << "  " << program_name << " -f avi --deinterlace capture.u64raw video.avi\n";
}

void printProgress(const EncoderProgress& progress) {
    // Clear line and print progress
    std::cout << "\r";
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "Progress: " << progress.getProgressPercent() << "% ";
    std::cout << "(" << progress.processed_frames << "/" << progress.total_frames << " frames) ";
    std::cout << "Speed: " << progress.fps << " fps ";
    std::cout << "ETA: " << progress.getTimeRemaining() << " ";
    std::cout << "Size: " << (progress.output_size_bytes / 1024 / 1024) << " MB";
    std::cout.flush();
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    
    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }
    
    EncoderConfig config;
    std::string input_file;
    std::string output_file;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
        else if (arg == "-f" || arg == "--format") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires a value\n";
                return 1;
            }
            std::string format = argv[i];
            if (format == "mp4") config.format = EncoderConfig::Format::MP4;
            else if (format == "avi") config.format = EncoderConfig::Format::AVI;
            else if (format == "mov") config.format = EncoderConfig::Format::MOV;
            else {
                std::cerr << "Error: Unknown format '" << format << "'\n";
                return 1;
            }
        }
        else if (arg == "-q" || arg == "--quality") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires a value\n";
                return 1;
            }
            std::string quality = argv[i];
            if (quality == "fast") config.quality = EncoderConfig::Quality::FAST;
            else if (quality == "balanced") config.quality = EncoderConfig::Quality::BALANCED;
            else if (quality == "quality") config.quality = EncoderConfig::Quality::QUALITY;
            else if (quality == "lossless") config.quality = EncoderConfig::Quality::LOSSLESS;
            else {
                std::cerr << "Error: Unknown quality '" << quality << "'\n";
                return 1;
            }
        }
        else if (arg == "-vb" || arg == "--video-bitrate") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires a value\n";
                return 1;
            }
            config.video_bitrate = std::atoi(argv[i]);
        }
        else if (arg == "-ab" || arg == "--audio-bitrate") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires a value\n";
                return 1;
            }
            config.audio_bitrate = std::atoi(argv[i]);
        }
        else if (arg == "-fps" || arg == "--framerate") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires a value\n";
                return 1;
            }
            config.video_fps = std::atoi(argv[i]);
        }
        else if (arg == "-t" || arg == "--threads") {
            if (++i >= argc) {
                std::cerr << "Error: " << arg << " requires a value\n";
                return 1;
            }
            config.threads = std::atoi(argv[i]);
        }
        else if (arg == "--no-gpu") {
            config.gpu_acceleration = false;
        }
        else if (arg == "--deinterlace") {
            config.deinterlace = true;
        }
        else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return 1;
        }
        else {
            // Positional arguments
            if (input_file.empty()) {
                input_file = arg;
            } else if (output_file.empty()) {
                output_file = arg;
            } else {
                std::cerr << "Error: Too many arguments\n";
                return 1;
            }
        }
    }
    
    if (input_file.empty() || output_file.empty()) {
        std::cerr << "Error: Input and output files required\n";
        printUsage(argv[0]);
        return 1;
    }
    
    config.input_path = input_file;
    config.output_path = output_file;
    
    // Validate input file
    if (!RawEncoder::validateInputFile(input_file)) {
        std::cerr << "Error: Invalid input file '" << input_file << "'\n";
        return 1;
    }
    
    // Analyze input file
    std::cout << "Analyzing input file..." << std::endl;
    auto analysis = RawEncoder::analyzeFile(input_file);
    
    std::cout << "Input file: " << input_file << std::endl;
    std::cout << "Duration: " << analysis.duration_seconds << " seconds" << std::endl;
    std::cout << "Video frames: " << analysis.total_frames << std::endl;
    std::cout << "Audio packets: " << analysis.total_audio_packets << std::endl;
    std::cout << "File size: " << (analysis.input_size_bytes / 1024 / 1024) << " MB" << std::endl;
    std::cout << std::endl;
    
    // Initialize encoder
    RawEncoder encoder;
    if (!encoder.initialize(config)) {
        std::cerr << "Error: Failed to initialize encoder\n";
        return 1;
    }
    
    // Set progress callback
    encoder.setProgressCallback([](const EncoderProgress& progress) {
        if (!should_cancel.load()) {
            printProgress(progress);
        }
    });
    
    std::cout << "Starting encoding..." << std::endl;
    
    // Start encoding
    if (!encoder.encodeAsync()) {
        std::cerr << "Error: Failed to start encoding\n";
        return 1;
    }
    
    // Wait for completion or cancellation
    while (encoder.isEncoding() && !should_cancel.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    if (should_cancel.load()) {
        encoder.cancel();
        std::cout << "\nEncoding cancelled.\n";
        return 1;
    }
    
    auto final_progress = encoder.getProgress();
    std::cout << std::endl;
    
    if (final_progress.has_error) {
        std::cerr << "Encoding failed: " << final_progress.error_message << std::endl;
        return 1;
    }
    
    std::cout << "Encoding completed successfully!" << std::endl;
    std::cout << "Output file: " << output_file << std::endl;
    std::cout << "Output size: " << (final_progress.output_size_bytes / 1024 / 1024) << " MB" << std::endl;
    std::cout << "Compression ratio: " << std::fixed << std::setprecision(1) 
              << final_progress.compression_ratio << ":1" << std::endl;
    
    return 0;
}
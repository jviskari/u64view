#include "raw_encoder.h"
#include "../src/config.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <atomic>
#include <mutex>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/channel_layout.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

// Global cancellation flag for the encoder
static std::atomic<bool> should_cancel{false};

// Real FFmpeg context
struct RawEncoder::FFmpegContext {
    // Format context
    AVFormatContext* format_ctx = nullptr;
    
    // Video encoding
    AVCodecContext* video_codec_ctx = nullptr;
    AVStream* video_stream = nullptr;
    AVFrame* video_frame = nullptr;
    AVPacket* video_packet = nullptr;
    struct SwsContext* sws_ctx = nullptr;
    
    // Audio encoding
    AVCodecContext* audio_codec_ctx = nullptr;
    AVStream* audio_stream = nullptr;
    AVFrame* audio_frame = nullptr;
    AVPacket* audio_packet = nullptr;
    struct SwrContext* swr_ctx = nullptr;
    
    // Encoding state
    int64_t video_pts = 0;
    int64_t audio_pts = 0;
    size_t frames_encoded = 0;
    size_t audio_samples_encoded = 0;
    
    // Progress tracking
    std::mutex progress_mutex;
    EncoderProgress current_progress;
    std::atomic<bool> is_encoding{false};
    
    bool initialized = false;
};

RawEncoder::RawEncoder() : ffmpeg_ctx_(std::make_unique<FFmpegContext>()) {
}

RawEncoder::~RawEncoder() {
    cleanup();
}

bool RawEncoder::initialize(const EncoderConfig& config) {
    config_ = config;
    
    std::cout << "Initializing encoder with FFmpeg..." << std::endl;
    std::cout << "Input: " << config.input_path << std::endl;
    std::cout << "Output: " << config.output_path << std::endl;
    
    // Validate input file exists
    if (!std::filesystem::exists(config.input_path)) {
        std::cerr << "Input file does not exist: " << config.input_path << std::endl;
        return false;
    }
    
    return setupFFmpeg();
}

bool RawEncoder::setupFFmpeg() {
    auto& ctx = *ffmpeg_ctx_;
    
    std::cout << "Setting up FFmpeg context..." << std::endl;
    
    // Determine output format from file extension
    std::string format_name;
    std::filesystem::path output_path(config_.output_path);
    std::string extension = output_path.extension().string();
    
    if (extension == ".mp4") {
        format_name = "mp4";
    } else if (extension == ".avi") {
        format_name = "avi";
    } else if (extension == ".mov") {
        format_name = "mov";
    } else {
        std::cerr << "Unsupported output format: " << extension << std::endl;
        return false;
    }
    
    // Allocate format context
    int ret = avformat_alloc_output_context2(&ctx.format_ctx, nullptr, 
                                           format_name.c_str(), config_.output_path.c_str());
    if (ret < 0) {
        std::cerr << "Could not allocate output format context" << std::endl;
        return false;
    }
    
    // Setup video encoder
    if (!setupVideoEncoder()) {
        return false;
    }
    
    // Setup audio encoder
    if (!setupAudioEncoder()) {
        return false;
    }
    
    // Open output file
    if (!(ctx.format_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ctx.format_ctx->pb, config_.output_path.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            std::cerr << "Could not open output file: " << config_.output_path << std::endl;
            return false;
        }
    }
    
    // Write file header
    ret = avformat_write_header(ctx.format_ctx, nullptr);
    if (ret < 0) {
        std::cerr << "Error writing header" << std::endl;
        return false;
    }
    
    std::cout << "FFmpeg setup completed successfully" << std::endl;
    ctx.initialized = true;
    return true;
}

bool RawEncoder::setupVideoEncoder() {
    auto& ctx = *ffmpeg_ctx_;
    
    // Find video encoder
    const AVCodec* video_codec = nullptr;
    std::string codec_name;
    
    switch (config_.format) {
        case EncoderConfig::Format::MP4:
        case EncoderConfig::Format::MOV:
            codec_name = "libx264";
            break;
        case EncoderConfig::Format::AVI:
            codec_name = "libx264"; // Can also use libxvid
            break;
    }
    
    video_codec = avcodec_find_encoder_by_name(codec_name.c_str());
    if (!video_codec) {
        std::cerr << "Video codec '" << codec_name << "' not found" << std::endl;
        return false;
    }
    
    // Create video stream
    ctx.video_stream = avformat_new_stream(ctx.format_ctx, nullptr);
    if (!ctx.video_stream) {
        std::cerr << "Could not allocate video stream" << std::endl;
        return false;
    }
    
    // Allocate codec context
    ctx.video_codec_ctx = avcodec_alloc_context3(video_codec);
    if (!ctx.video_codec_ctx) {
        std::cerr << "Could not allocate video codec context" << std::endl;
        return false;
    }
    
    // Set video parameters
    ctx.video_codec_ctx->codec_id = video_codec->id;
    ctx.video_codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    ctx.video_codec_ctx->width = Config::Video::FRAME_WIDTH;
    ctx.video_codec_ctx->height = Config::Video::FRAME_HEIGHT;
    ctx.video_codec_ctx->time_base = {1, config_.video_fps};
    ctx.video_codec_ctx->framerate = {config_.video_fps, 1};
    ctx.video_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    ctx.video_codec_ctx->bit_rate = config_.video_bitrate * 1000; // Convert kbps to bps
    
    // Set quality/speed preset
    switch (config_.quality) {
        case EncoderConfig::Quality::FAST:
            av_opt_set(ctx.video_codec_ctx->priv_data, "preset", "ultrafast", 0);
            av_opt_set(ctx.video_codec_ctx->priv_data, "crf", "28", 0);
            break;
        case EncoderConfig::Quality::BALANCED:
            av_opt_set(ctx.video_codec_ctx->priv_data, "preset", "medium", 0);
            av_opt_set(ctx.video_codec_ctx->priv_data, "crf", "23", 0);
            break;
        case EncoderConfig::Quality::QUALITY:
            av_opt_set(ctx.video_codec_ctx->priv_data, "preset", "slow", 0);
            av_opt_set(ctx.video_codec_ctx->priv_data, "crf", "18", 0);
            break;
        case EncoderConfig::Quality::LOSSLESS:
            av_opt_set(ctx.video_codec_ctx->priv_data, "preset", "medium", 0);
            av_opt_set(ctx.video_codec_ctx->priv_data, "crf", "0", 0);
            break;
    }
    
    // Some formats want stream headers to be separate
    if (ctx.format_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        ctx.video_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    
    // Open video codec
    int ret = avcodec_open2(ctx.video_codec_ctx, video_codec, nullptr);
    if (ret < 0) {
        std::cerr << "Could not open video codec" << std::endl;
        return false;
    }
    
    // Copy codec parameters to stream
    ret = avcodec_parameters_from_context(ctx.video_stream->codecpar, ctx.video_codec_ctx);
    if (ret < 0) {
        std::cerr << "Could not copy video codec parameters" << std::endl;
        return false;
    }
    
    ctx.video_stream->time_base = ctx.video_codec_ctx->time_base;
    
    // Allocate video frame
    ctx.video_frame = av_frame_alloc();
    if (!ctx.video_frame) {
        std::cerr << "Could not allocate video frame" << std::endl;
        return false;
    }
    
    ctx.video_frame->format = ctx.video_codec_ctx->pix_fmt;
    ctx.video_frame->width = ctx.video_codec_ctx->width;
    ctx.video_frame->height = ctx.video_codec_ctx->height;
    
    ret = av_frame_get_buffer(ctx.video_frame, 0);
    if (ret < 0) {
        std::cerr << "Could not allocate video frame buffer" << std::endl;
        return false;
    }
    
    // Allocate video packet
    ctx.video_packet = av_packet_alloc();
    if (!ctx.video_packet) {
        std::cerr << "Could not allocate video packet" << std::endl;
        return false;
    }
    
    // Setup scaling context (RGB24 to YUV420P)
    ctx.sws_ctx = sws_getContext(
        Config::Video::FRAME_WIDTH, Config::Video::FRAME_HEIGHT, AV_PIX_FMT_RGB24,
        Config::Video::FRAME_WIDTH, Config::Video::FRAME_HEIGHT, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    if (!ctx.sws_ctx) {
        std::cerr << "Could not create scaling context" << std::endl;
        return false;
    }
    
    std::cout << "Video encoder setup: " << codec_name << " " 
              << Config::Video::FRAME_WIDTH << "x" << Config::Video::FRAME_HEIGHT 
              << " @ " << config_.video_fps << " fps" << std::endl;
    
    return true;
}

bool RawEncoder::setupAudioEncoder() {
    auto& ctx = *ffmpeg_ctx_;
    
    // Find audio encoder
    const AVCodec* audio_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!audio_codec) {
        std::cerr << "AAC codec not found" << std::endl;
        return false;
    }
    
    // Create audio stream
    ctx.audio_stream = avformat_new_stream(ctx.format_ctx, nullptr);
    if (!ctx.audio_stream) {
        std::cerr << "Could not allocate audio stream" << std::endl;
        return false;
    }
    
    // Allocate codec context
    ctx.audio_codec_ctx = avcodec_alloc_context3(audio_codec);
    if (!ctx.audio_codec_ctx) {
        std::cerr << "Could not allocate audio codec context" << std::endl;
        return false;
    }
    
    // Set audio parameters using new channel layout API
    ctx.audio_codec_ctx->codec_id = audio_codec->id;
    ctx.audio_codec_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
    ctx.audio_codec_ctx->sample_rate = config_.audio_sample_rate;
    
    // Create stereo channel layout properly
    AVChannelLayout stereo_layout = AV_CHANNEL_LAYOUT_STEREO;
    av_channel_layout_copy(&ctx.audio_codec_ctx->ch_layout, &stereo_layout);
    
    ctx.audio_codec_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP; // AAC prefers planar float
    ctx.audio_codec_ctx->bit_rate = config_.audio_bitrate * 1000;
    ctx.audio_codec_ctx->time_base = {1, config_.audio_sample_rate};
    
    // Some formats want stream headers to be separate
    if (ctx.format_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        ctx.audio_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    
    // Open audio codec
    int ret = avcodec_open2(ctx.audio_codec_ctx, audio_codec, nullptr);
    if (ret < 0) {
        std::cerr << "Could not open audio codec" << std::endl;
        return false;
    }
    
    // Copy codec parameters to stream
    ret = avcodec_parameters_from_context(ctx.audio_stream->codecpar, ctx.audio_codec_ctx);
    if (ret < 0) {
        std::cerr << "Could not copy audio codec parameters" << std::endl;
        return false;
    }
    
    ctx.audio_stream->time_base = ctx.audio_codec_ctx->time_base;
    
    // Allocate audio frame
    ctx.audio_frame = av_frame_alloc();
    if (!ctx.audio_frame) {
        std::cerr << "Could not allocate audio frame" << std::endl;
        return false;
    }
    
    ctx.audio_frame->format = ctx.audio_codec_ctx->sample_fmt;
    av_channel_layout_copy(&ctx.audio_frame->ch_layout, &ctx.audio_codec_ctx->ch_layout);
    ctx.audio_frame->sample_rate = ctx.audio_codec_ctx->sample_rate;
    ctx.audio_frame->nb_samples = ctx.audio_codec_ctx->frame_size;
    
    ret = av_frame_get_buffer(ctx.audio_frame, 0);
    if (ret < 0) {
        std::cerr << "Could not allocate audio frame buffer" << std::endl;
        return false;
    }
    
    // Allocate audio packet
    ctx.audio_packet = av_packet_alloc();
    if (!ctx.audio_packet) {
        std::cerr << "Could not allocate audio packet" << std::endl;
        return false;
    }
    
    // Setup resampling context using new API
    AVChannelLayout out_layout = AV_CHANNEL_LAYOUT_STEREO;
    AVChannelLayout in_layout = AV_CHANNEL_LAYOUT_STEREO;
    
    ret = swr_alloc_set_opts2(&ctx.swr_ctx,
        &out_layout, AV_SAMPLE_FMT_FLTP, config_.audio_sample_rate,
        &in_layout, AV_SAMPLE_FMT_S16, Config::Audio::SAMPLE_RATE,
        0, nullptr);
    
    if (ret < 0 || swr_init(ctx.swr_ctx) < 0) {
        std::cerr << "Could not create resampling context" << std::endl;
        return false;
    }
    
    std::cout << "Audio encoder setup: AAC " << config_.audio_sample_rate 
              << " Hz stereo @ " << config_.audio_bitrate << " kbps" << std::endl;
    
    return true;
}

void RawEncoder::setProgressCallback(ProgressCallback callback) {
    progress_callback_ = callback;
}

bool RawEncoder::encode() {
    if (!ffmpeg_ctx_->initialized) {
        std::cerr << "Encoder not initialized" << std::endl;
        return false;
    }
    
    auto& ctx = *ffmpeg_ctx_;
    ctx.is_encoding = true;
    
    std::cout << "Starting encoding process..." << std::endl;
    
    // Analyze input file first
    auto analysis = analyzeFile(config_.input_path);
    if (analysis.total_frames == 0) {
        std::cerr << "No frames found in input file" << std::endl;
        ctx.is_encoding = false;
        return false;
    }
    
    // Initialize progress
    {
        std::lock_guard<std::mutex> lock(ctx.progress_mutex);
        ctx.current_progress = EncoderProgress{};
        ctx.current_progress.total_frames = analysis.total_frames;
        ctx.current_progress.total_audio_packets = analysis.total_audio_packets;
        ctx.current_progress.duration_seconds = analysis.duration_seconds;
        ctx.current_progress.input_size_bytes = analysis.input_size_bytes;
    }
    
    std::cout << "Processing " << analysis.total_frames << " frames..." << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (size_t frame = 0; frame < analysis.total_frames && !should_cancel.load(); frame++) {
        // Create test pattern (for now)
        if (!encodeTestFrame(frame)) {
            std::cerr << "Failed to encode frame " << frame << std::endl;
            ctx.is_encoding = false;
            return false;
        }
        
        // Update progress
        {
            std::lock_guard<std::mutex> lock(ctx.progress_mutex);
            ctx.current_progress.processed_frames = frame + 1;
            
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
            ctx.current_progress.processed_seconds = elapsed.count() / 1000.0;
            
            if (elapsed.count() > 0) {
                ctx.current_progress.fps = (ctx.current_progress.processed_frames * 1000.0) / elapsed.count();
            }
            
            ctx.current_progress.status_message = "Encoding frame " + std::to_string(frame + 1);
        }
        
        // Call progress callback
        if (progress_callback_) {
            std::lock_guard<std::mutex> lock(ctx.progress_mutex);
            progress_callback_(ctx.current_progress);
        }
    }
    
    if (should_cancel.load()) {
        std::cout << "Encoding cancelled" << std::endl;
        ctx.is_encoding = false;
        return false;
    }
    
    // Write trailer
    av_write_trailer(ctx.format_ctx);
    
    // Final progress update
    {
        std::lock_guard<std::mutex> lock(ctx.progress_mutex);
        ctx.current_progress.is_complete = true;
        ctx.current_progress.status_message = "Encoding completed";
        
        // Get output file size
        if (std::filesystem::exists(config_.output_path)) {
            ctx.current_progress.output_size_bytes = std::filesystem::file_size(config_.output_path);
            if (ctx.current_progress.input_size_bytes > 0) {
                ctx.current_progress.compression_ratio = 
                    static_cast<double>(ctx.current_progress.input_size_bytes) / ctx.current_progress.output_size_bytes;
            }
        }
    }
    
    if (progress_callback_) {
        std::lock_guard<std::mutex> lock(ctx.progress_mutex);
        progress_callback_(ctx.current_progress);
    }
    
    ctx.is_encoding = false;
    std::cout << "Encoding completed successfully!" << std::endl;
    return true;
}

bool RawEncoder::encodeTestFrame(size_t frame_num) {
    auto& ctx = *ffmpeg_ctx_;
    
    // Make frame writable
    int ret = av_frame_make_writable(ctx.video_frame);
    if (ret < 0) {
        return false;
    }
    
    // Create simple test pattern
    for (int y = 0; y < ctx.video_codec_ctx->height; y++) {
        for (int x = 0; x < ctx.video_codec_ctx->width; x++) {
            // Simple animated pattern
            int color = (x + y + frame_num) % 256;
            ctx.video_frame->data[0][y * ctx.video_frame->linesize[0] + x] = color;
        }
    }
    
    // Fill U and V planes with neutral values
    for (int y = 0; y < ctx.video_codec_ctx->height / 2; y++) {
        for (int x = 0; x < ctx.video_codec_ctx->width / 2; x++) {
            ctx.video_frame->data[1][y * ctx.video_frame->linesize[1] + x] = 128;
            ctx.video_frame->data[2][y * ctx.video_frame->linesize[2] + x] = 128;
        }
    }
    
    ctx.video_frame->pts = ctx.video_pts++;
    
    // Encode frame
    ret = avcodec_send_frame(ctx.video_codec_ctx, ctx.video_frame);
    if (ret < 0) {
        std::cerr << "Error sending frame to encoder" << std::endl;
        return false;
    }
    
    while (ret >= 0) {
        ret = avcodec_receive_packet(ctx.video_codec_ctx, ctx.video_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            std::cerr << "Error encoding frame" << std::endl;
            return false;
        }
        
        // Rescale packet timestamps
        av_packet_rescale_ts(ctx.video_packet, ctx.video_codec_ctx->time_base, ctx.video_stream->time_base);
        ctx.video_packet->stream_index = ctx.video_stream->index;
        
        // Write packet
        ret = av_interleaved_write_frame(ctx.format_ctx, ctx.video_packet);
        if (ret < 0) {
            std::cerr << "Error writing frame" << std::endl;
            return false;
        }
        
        av_packet_unref(ctx.video_packet);
    }
    
    return true;
}

bool RawEncoder::encodeAsync() {
    // Launch encoding in background thread
    std::thread([this]() {
        encode();
    }).detach();
    
    return true;
}

EncoderProgress RawEncoder::getProgress() const {
    std::lock_guard<std::mutex> lock(ffmpeg_ctx_->progress_mutex);
    return ffmpeg_ctx_->current_progress;
}

bool RawEncoder::isEncoding() const {
    return ffmpeg_ctx_->is_encoding.load();
}

void RawEncoder::cancel() {
    should_cancel = true;
}

bool RawEncoder::validateInputFile(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        return false;
    }
    
    // Check file extension
    std::filesystem::path file_path(path);
    std::string extension = file_path.extension().string();
    return extension == ".u64raw";
}

EncoderProgress RawEncoder::analyzeFile(const std::string& path) {
    EncoderProgress analysis;
    
    if (!std::filesystem::exists(path)) {
        analysis.has_error = true;
        analysis.error_message = "File does not exist";
        return analysis;
    }
    
    analysis.input_size_bytes = std::filesystem::file_size(path);
    
    // Simple estimation based on file size
    std::cout << "Estimating file contents based on size..." << std::endl;
    
    // Rough estimation: assume ~300KB per frame (video + audio)
    analysis.total_frames = analysis.input_size_bytes / (300 * 1024);
    if (analysis.total_frames == 0) {
        analysis.total_frames = 100; // Minimum assumption
    }
    
    analysis.total_audio_packets = analysis.total_frames * 5; // ~5 audio packets per frame
    analysis.duration_seconds = analysis.total_frames / 50.0; // 50 FPS
    
    std::cout << "Estimated " << analysis.total_frames << " frames, " 
              << analysis.duration_seconds << " seconds" << std::endl;
    
    return analysis;
}

std::vector<std::string> RawEncoder::getSupportedFormats() {
    return {"mp4", "avi", "mov"};
}

bool RawEncoder::processVideoFrame(const VideoFrame& /* frame */) {
    // Dummy video frame processing
    ffmpeg_ctx_->frames_encoded++;
    return true;
}

bool RawEncoder::processAudioPacket(const AudioPacket& /* packet */) {
    // Dummy audio packet processing
    ffmpeg_ctx_->audio_samples_encoded++;
    return true;
}

void RawEncoder::updateProgress() {
    // Update progress tracking
}

void RawEncoder::cleanup() {
    if (!ffmpeg_ctx_) return;
    
    auto& ctx = *ffmpeg_ctx_;
    ctx.is_encoding = false;
    
    // Clean up video resources
    if (ctx.video_frame) {
        av_frame_free(&ctx.video_frame);
    }
    if (ctx.video_packet) {
        av_packet_free(&ctx.video_packet);
    }
    if (ctx.video_codec_ctx) {
        avcodec_free_context(&ctx.video_codec_ctx);
    }
    if (ctx.sws_ctx) {
        sws_freeContext(ctx.sws_ctx);
    }
    
    // Clean up audio resources
    if (ctx.audio_frame) {
        av_frame_free(&ctx.audio_frame);
    }
    if (ctx.audio_packet) {
        av_packet_free(&ctx.audio_packet);
    }
    if (ctx.audio_codec_ctx) {
        avcodec_free_context(&ctx.audio_codec_ctx);
    }
    if (ctx.swr_ctx) {
        swr_free(&ctx.swr_ctx);
    }
    
    // Clean up format context
    if (ctx.format_ctx) {
        if (ctx.format_ctx->pb) {
            avio_closep(&ctx.format_ctx->pb);
        }
        avformat_free_context(ctx.format_ctx);
    }
    
    ctx.initialized = false;
}
# Ultimate 64 Video Stream Viewer (Swift/macOS)

A native macOS Swift port of the Ultimate 64 video stream viewer using SwiftUI and native macOS frameworks.

## Features

- **Native macOS Integration**: Built with SwiftUI and AppKit
- **High Performance**: Real-time 50Hz PAL frame rate with optimized rendering
- **Low Latency**: Smart jitter buffering with frame skipping for minimal delay
- **VIC-II Color Palette**: Authentic Commodore 64 colors
- **Memory Safe**: Swift's automatic memory management
- **Modern UI**: Clean SwiftUI interface with status indicators
- **Standard macOS Behavior**: Cmd+Q to quit, red button closes window

## Requirements

- macOS 13.0 or later
- Swift 5.9 or later
- Xcode 14.0 or later (for development)

## Building

### Quick Build

```bash
cd swift
swift build -c release
```

### Development Build

```bash
cd swift
swift build
```

## Running

```bash
swift run -c release Ultimate64Viewer
```

Or run the built executable:

```bash
.build/release/Ultimate64Viewer
```

## Usage

1. **Start the Ultimate 64** with video streaming enabled
2. **Launch the viewer** - it will automatically start receiving
3. **View the stream** - displays at ~50fps with status information
4. **Quit the app** using Cmd+Q or the red close button

## Configuration

The following constants can be modified in the source code:

- **Multicast Group**: `239.0.1.64` (in `NetworkReceiver.swift`)
- **Port**: `11000` (in `NetworkReceiver.swift`)
- **Display Rate**: `60 Hz` (in `Ultimate64ViewModel.swift`)
- **Jitter Buffer**: `1 frame` (in `Ultimate64ViewModel.swift`)

## Architecture

### Core Components

- **NetworkReceiver**: UDP multicast reception with BSD sockets
- **FrameProcessor**: Thread-safe packet assembly into complete frames
- **Ultimate64ViewModel**: Main actor managing app state and frame buffering
- **VideoDisplayView**: SwiftUI view rendering frames via NSImage/CGContext
- **StatusBarView**: Connection status and performance metrics

### Performance Optimizations

1. **Low Latency Buffering**: Minimal 1-frame jitter buffer
2. **Frame Skipping**: Drops old frames when queue fills up
3. **Efficient Rendering**: Direct RGB→RGBA conversion with CGContext
4. **Thread Safety**: Lock-protected frame processor with async UI updates
5. **Release Builds**: Optimized compilation for production use

## Protocol Compatibility

Fully compatible with the Ultimate 64 video stream protocol:
- UDP multicast on `239.0.1.64:11000`
- 12-byte packet headers with sequence/frame/line info
- 4-bit indexed color pixels (2 pixels per byte)
- 384×272 frame resolution at 50Hz PAL
- 68 packets per frame (4 lines per packet)

## Troubleshooting

### Network Issues
- Ensure multicast traffic is allowed on your network
- Check firewall settings for UDP port 11000
- Verify the Ultimate 64 is broadcasting on the correct multicast group

### Performance Issues
- The app maintains ~50fps automatically
- FPS counter shows actual reception rate
- Frame skipping prevents lag buildup

### Build Issues
- Ensure you have Swift 5.9+ installed
- Use `swift --version` to check your Swift version
- Try `swift package clean` if builds fail

## Development

### Project Structure
```
Sources/Ultimate64Viewer/
├── Ultimate64ViewerApp.swift     # Main app entry point
├── Models/
│   ├── Ultimate64ViewModel.swift # Main view model
│   ├── ProcessedFrame.swift      # Frame data structure
│   └── PacketHeader.swift        # Network packet parsing
├── Network/
│   └── NetworkReceiver.swift     # UDP multicast receiver
├── Rendering/
│   ├── FrameProcessor.swift      # Packet→frame assembly
│   └── VICIIPalette.swift       # C64 color palette
└── Views/
    ├── ContentView.swift         # Main UI layout
    ├── VideoDisplayView.swift    # Video rendering
    └── StatusBarView.swift       # Status information
```

### Adding Features

- **Recording**: Extend `FrameProcessor` to save frames to disk
- **Filters**: Add image processing in `ProcessedFrame.createNSImage()`
- **Network Config**: Make multicast settings user-configurable
- **Multiple Streams**: Support multiple Ultimate 64 devices
- **Fullscreen**: Add fullscreen viewing mode

### Key Improvements over C++ Version

1. **Memory Safety**: No manual memory management or buffer overflows
2. **Modern Concurrency**: Swift's MainActor and async/await
3. **Robust Networking**: Proper error handling and recovery
4. **Native UI**: SwiftUI with proper macOS integration
5. **Better Performance**: Optimized frame processing and rendering
6. **Maintainability**: Clean, modular Swift code

## License

This project follows the same license as the original C++ implementation.
# Ultimate 64 Video Stream Viewer (Swift/macOS)

A native macOS Swift port of the Ultimate 64 video stream viewer using SwiftUI and native macOS frameworks.

## Features

- **Native macOS Integration**: Built with SwiftUI and AppKit
- **Network Framework**: Modern Swift networking with robust error handling
- **Real-time Performance**: 50Hz PAL frame rate with jitter buffering
- **VIC-II Color Palette**: Authentic Commodore 64 colors
- **Memory Safe**: Swift's automatic memory management
- **Modern UI**: Clean SwiftUI interface with status indicators

## Requirements

- macOS 13.0 or later
- Xcode 14.0 or later (for development)
- Swift 5.9 or later

## Building

### Using Swift Package Manager

```bash
cd swift
chmod +x build.sh
./build.sh
```

### Manual Build

```bash
cd swift
swift build -c release
```

## Running

```bash
swift run Ultimate64Viewer
```

Or run the built executable:

```bash
.build/release/Ultimate64Viewer
```

## Configuration

The following constants can be modified in the source code:

- **Multicast Group**: `239.0.1.64` (in `NetworkReceiver.swift`)
- **Port**: `11000` (in `NetworkReceiver.swift`)
- **Frame Rate**: `50 Hz` (in `Ultimate64ViewModel.swift`)
- **Jitter Buffer**: `2 frames` (in `Ultimate64ViewModel.swift`)

## Architecture

### Components

- **NetworkReceiver**: Handles UDP multicast reception using Network framework
- **FrameProcessor**: Assembles packets into complete frames
- **Ultimate64ViewModel**: Manages application state and frame buffering
- **FrameView**: Renders video frames using NSImageView
- **ContentView**: Main SwiftUI interface

### Key Improvements over C++ Version

1. **Memory Safety**: No manual memory management
2. **Modern Concurrency**: Swift's async/await and MainActor
3. **Robust Networking**: Network framework with automatic error recovery
4. **Native UI**: SwiftUI with proper macOS integration
5. **Better Error Handling**: Comprehensive error reporting

## Protocol Compatibility

Fully compatible with the original Ultimate 64 video stream protocol:
- UDP multicast on 239.0.1.64:11000
- 12-byte packet headers
- 4-bit indexed color pixels (2 per byte)
- 384×272 frame resolution
- 68 packets per frame

## Troubleshooting

### Network Issues
- Ensure multicast traffic is allowed on your network
- Check firewall settings for UDP port 11000
- Verify the Ultimate 64 is broadcasting on the correct multicast group

### Performance Issues
- The app automatically manages frame buffering
- FPS counter shows actual reception rate
- Jitter buffer smooths out network irregularities

## Development

To modify or extend the viewer:

1. Open the project in Xcode: `open Package.swift`
2. Or use any Swift-compatible editor
3. The modular architecture makes it easy to add features

### Adding Features

- **Recording**: Extend `FrameProcessor` to save frames
- **Filters**: Add image processing in `ProcessedFrame`
- **Network Config**: Make multicast settings configurable
- **Multiple Streams**: Support multiple Ultimate 64 devices
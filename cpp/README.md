# Ultimate 64 Video Stream Viewer (C++/SDL2)

A C++ port of the Ultimate 64 video stream viewer using SDL2 for cross-platform compatibility.

## Features

- Real-time UDP multicast video streaming reception
- VIC-II color palette support
- Jitter buffer for smooth playback
- Cross-platform support (Windows, Linux, macOS)
- 50Hz PAL frame rate support

## Dependencies

- SDL2 development libraries
- CMake 3.10 or higher
- C++17 compatible compiler

## Building

### Linux/macOS

Install SDL2 development packages:

**Ubuntu/Debian:**
```bash
sudo apt-get install libsdl2-dev cmake build-essential pkg-config
```

**macOS (with Homebrew):**
```bash
brew install sdl2 cmake
```

**Build:**
```bash
chmod +x build.sh
./build.sh
```

### Windows

1. Install SDL2 development libraries from https://www.libsdl.org/
2. Use CMake GUI or command line to configure and build
3. Ensure SDL2.dll is in your PATH or copy it to the executable directory

### Manual Build

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

## Running

```bash
./build/Ultimate64Viewer
```

Press ESC to exit the viewer.

## Configuration

The following constants can be modified in `src/main.cpp`:

- `MULTICAST_GROUP`: Multicast IP address (default: "239.0.1.64")
- `SERVER_PORT`: UDP port (default: 11000)
- `FRAME_INTERVAL`: Frame rate (default: 1/50 for PAL)
- `JITTER_BUFFER_FRAMES`: Buffer depth for smoothing (default: 2)

## Protocol

The viewer expects UDP packets with the following structure:
- Header: 12 bytes (seq, frm, lin, width, lp, bp, enc)
- Payload: Pixel data (4-bit indexed color, 2 pixels per byte)
- Frame format: 384x272 pixels, 68 packets per frame

## Docker

Build and run with Docker:

```bash
docker build -t ultimate64-viewer .
docker run --rm --net=host -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix ultimate64-viewer
```

Note: Docker networking and X11 forwarding may require additional configuration depending on your system.
```

Now let me create the build script and make it executable:

```bash
chmod +x build.sh

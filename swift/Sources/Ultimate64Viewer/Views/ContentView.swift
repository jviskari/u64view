import SwiftUI

struct ContentView: View {
    @StateObject private var viewModel = Ultimate64ViewModel()
    @State private var showingInfo = false
    
    var body: some View {
        VStack(spacing: 0) {
            // Main video display
            ZStack {
                Color.black
                
                if let currentFrame = viewModel.currentFrame {
                    FrameView(frame: currentFrame)
                        .aspectRatio(384.0/272.0, contentMode: .fit)
                } else {
                    VStack {
                        Image(systemName: "tv")
                            .font(.system(size: 48))
                            .foregroundColor(.gray)
                        Text("Waiting for video stream...")
                            .foregroundColor(.gray)
                            .padding(.top)
                    }
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            
            // Status bar
            HStack {
                HStack {
                    Circle()
                        .fill(viewModel.isReceiving ? Color.green : Color.red)
                        .frame(width: 8, height: 8)
                    Text(viewModel.isReceiving ? "Receiving" : "Disconnected")
                        .font(.caption)
                }
                
                Spacer()
                
                Text("Frame: \(viewModel.frameNumber)")
                    .font(.caption)
                    .monospacedDigit()
                
                Spacer()
                
                Text("FPS: \(viewModel.fps, specifier: "%.1f")")
                    .font(.caption)
                    .monospacedDigit()
                
                Spacer()
                
                Button("Info") {
                    showingInfo.toggle()
                }
                .font(.caption)
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 8)
            .background(Color(NSColor.controlBackgroundColor))
            
            // Error message
            if let errorMessage = viewModel.errorMessage {
                Text(errorMessage)
                    .foregroundColor(.red)
                    .font(.caption)
                    .padding(.horizontal, 12)
                    .padding(.vertical, 4)
                    .background(Color(NSColor.controlBackgroundColor))
            }
        }
        .onAppear {
            viewModel.startReceiving()
        }
        .onDisappear {
            viewModel.stopReceiving()
        }
        .sheet(isPresented: $showingInfo) {
            InfoView()
        }
    }
}

struct InfoView: View {
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Ultimate 64 Video Stream Viewer")
                .font(.title2)
                .bold()
            
            Text("Swift/macOS Native Version")
                .font(.headline)
            
            Divider()
            
            VStack(alignment: .leading, spacing: 8) {
                Text("Configuration:")
                    .font(.headline)
                
                Text("• Multicast Group: 239.0.1.64")
                Text("• Port: 11000")
                Text("• Frame Rate: 50 Hz (PAL)")
                Text("• Resolution: 384×272")
                Text("• Color Palette: VIC-II (16 colors)")
            }
            .font(.caption)
            
            Divider()
            
            Text("Press ESC or close window to exit")
                .font(.caption)
                .foregroundColor(.secondary)
            
            Spacer()
        }
        .padding()
        .frame(width: 300, height: 250)
    }
}
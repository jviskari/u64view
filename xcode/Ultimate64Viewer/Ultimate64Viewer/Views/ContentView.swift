import SwiftUI

struct ContentView: View {
    @StateObject private var viewModel = Ultimate64ViewModel()
    @State private var animationPhase = 0.0
    
    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            
            if let frame = viewModel.currentFrame {
                #if canImport(UIKit)
                if let uiImage = frame.createImage() as? UIImage {
                    Image(uiImage: uiImage)
                        .resizable()
                        .aspectRatio(contentMode: .fit)
                }
                #elseif canImport(AppKit)
                if let nsImage = frame.createImage() as? NSImage {
                    Image(nsImage: nsImage)
                        .resizable()
                        .aspectRatio(contentMode: .fit)
                }
                #endif
                
                // Show connection info overlay briefly when receiving
                if viewModel.showConnectionInfo {
                    VStack {
                        Spacer()
                        HStack {
                            Spacer()
                            VStack(alignment: .trailing, spacing: 4) {
                                if let sourceIP = viewModel.sourceIP {
                                    Text("📡 \(sourceIP)")
                                        .foregroundColor(.green)
                                        .font(.system(.caption, design: .monospaced))
                                }
                                Text("Frame \(viewModel.currentFrame?.number ?? 0)")
                                    .foregroundColor(.white.opacity(0.8))
                                    .font(.system(.caption, design: .monospaced))
                            }
                            .padding(8)
                            .background(Color.black.opacity(0.7))
                            .cornerRadius(6)
                            .padding()
                        }
                    }
                }
            } else {
                VStack(spacing: 24) {
                    // Animated loading indicator
                    HStack(spacing: 8) {
                        ForEach(0..<3) { index in
                            Circle()
                                .fill(Color.white)
                                .frame(width: 12, height: 12)
                                .opacity(0.3 + 0.7 * sin(animationPhase + Double(index) * 0.8))
                        }
                    }
                    .animation(.easeInOut(duration: 1.2).repeatForever(), value: animationPhase)
                    
                    VStack(spacing: 12) {
                        Text(viewModel.isConnected ? "Connection Lost" : "Waiting for Ultimate 64")
                            .foregroundColor(viewModel.isConnected ? .orange : .white)
                            .font(.title2)
                            .fontWeight(.medium)
                        
                        VStack(spacing: 8) {
                            Text("Listening on:")
                                .foregroundColor(.white.opacity(0.7))
                                .font(.subheadline)
                            
                            Text("239.0.1.64:11000")
                                .foregroundColor(.cyan)
                                .font(.system(.title3, design: .monospaced))
                                .fontWeight(.semibold)
                            
                            if let lastIP = viewModel.sourceIP {
                                Text("Last connected: \(lastIP)")
                                    .foregroundColor(.white.opacity(0.5))
                                    .font(.system(.caption, design: .monospaced))
                            }
                        }
                        
                        Text(viewModel.isConnected ? 
                             "Reconnecting..." : 
                             "Configure Ultimate 64 to send video to this multicast address")
                            .foregroundColor(.white.opacity(0.6))
                            .font(.caption)
                            .multilineTextAlignment(.center)
                            .padding(.horizontal, 40)
                    }
                }
                .onAppear {
                    withAnimation {
                        animationPhase = .pi * 2
                    }
                }
            }
        }
        .onAppear {
            viewModel.startReceiving()
        }
        .onDisappear {
            viewModel.stopReceiving()
        }
    }
}

#Preview {
    ContentView()
}
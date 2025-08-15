import SwiftUI

struct ContentView: View {
    @StateObject private var viewModel = Ultimate64ViewModel()
    
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
            } else {
                Text("Waiting for video stream...")
                    .foregroundColor(.white)
                    .font(.title2)
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
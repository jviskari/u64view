import SwiftUI

struct ContentView: View {
    @StateObject private var viewModel = Ultimate64ViewModel()
    
    var body: some View {
        VStack(spacing: 0) {
            VideoDisplayView(frame: viewModel.currentFrame)
            
            StatusBarView(
                isReceiving: viewModel.isReceiving,
                frameNumber: viewModel.frameNumber,
                fps: viewModel.fps,
                errorMessage: viewModel.errorMessage
            )
        }
        .frame(minWidth: 768, minHeight: 544)
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
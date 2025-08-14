import SwiftUI
import AppKit

struct VideoDisplayView: View {
    let frame: ProcessedFrame?
    
    var body: some View {
        ZStack {
            Color.black
            
            if let frame = frame {
                if let nsImage = frame.createNSImage() {
                    Image(nsImage: nsImage)
                        .resizable()
                        .aspectRatio(contentMode: .fit)
                        .onAppear {
                            print("Displaying image for frame \(frame.number), size: \(nsImage.size)")
                        }
                } else {
                    Text("Failed to create image for frame \(frame.number)")
                        .foregroundColor(.red)
                        .onAppear {
                            print("Failed to create NSImage for frame \(frame.number)")
                        }
                }
            } else {
                Text("Waiting for video stream...")
                    .foregroundColor(.white)
                    .onAppear {
                        print("No frame available for display")
                    }
            }
        }
    }
}
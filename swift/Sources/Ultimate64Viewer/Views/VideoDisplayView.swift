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
                } else {
                    Text("Failed to create image")
                        .foregroundColor(.red)
                }
            } else {
                Text("Waiting for video stream...")
                    .foregroundColor(.white)
            }
        }
    }
}
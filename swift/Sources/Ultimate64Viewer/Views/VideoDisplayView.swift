import SwiftUI

struct VideoDisplayView: View {
    let frame: ProcessedFrame?
    
    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            
            if let frame = frame {
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
    }
}
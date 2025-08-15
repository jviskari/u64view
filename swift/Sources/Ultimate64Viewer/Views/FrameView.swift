import SwiftUI
import AppKit

struct FrameView: NSViewRepresentable {
    let frame: ProcessedFrame?
    
    func makeNSView(context: Context) -> NSImageView {
        let imageView = NSImageView()
        imageView.imageScaling = .scaleProportionallyUpOrDown
        imageView.imageAlignment = .alignCenter
        return imageView
    }
    
    func updateNSView(_ nsView: NSImageView, context: Context) {
        if let frame = frame {
            nsView.image = frame.createImage() as? NSImage
        } else {
            nsView.image = nil
        }
    }
}
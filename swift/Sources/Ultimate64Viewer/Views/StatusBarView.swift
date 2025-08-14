import SwiftUI

struct StatusBarView: View {
    let isReceiving: Bool
    let frameNumber: UInt16
    let fps: Double
    let errorMessage: String?
    
    var body: some View {
        HStack {
            // Connection status
            HStack(spacing: 4) {
                Circle()
                    .fill(isReceiving ? Color.green : Color.red)
                    .frame(width: 8, height: 8)
                Text(isReceiving ? "Connected" : "Disconnected")
                    .font(.caption)
            }
            
            Spacer()
            
            // Frame info
            if isReceiving {
                Text("Frame: \(frameNumber)")
                    .font(.caption)
                    .foregroundColor(.secondary)
                
                Text("FPS: \(fps, specifier: "%.1f")")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            
            // Error message
            if let error = errorMessage {
                Text("Error: \(error)")
                    .font(.caption)
                    .foregroundColor(.red)
            }
        }
        .padding(.horizontal, 8)
        .padding(.vertical, 4)
        .background(Color(NSColor.controlBackgroundColor))
    }
}
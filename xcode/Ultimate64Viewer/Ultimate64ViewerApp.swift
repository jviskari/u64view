import SwiftUI

@main
struct Ultimate64ViewerApp: App {
    var body: some Scene {
        WindowGroup {
            ContentView()
        }
        #if canImport(AppKit)
        .windowResizability(.contentSize)
        #endif
    }
}
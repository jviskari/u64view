// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "Ultimate64Viewer",
    platforms: [
        .macOS(.v13)
    ],
    products: [
        .executable(
            name: "Ultimate64Viewer",
            targets: ["Ultimate64Viewer"]
        ),
    ],
    dependencies: [],
    targets: [
        .executableTarget(
            name: "Ultimate64Viewer",
            dependencies: []
        ),
    ]
)
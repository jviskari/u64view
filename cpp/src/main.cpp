#include "ultimate64_viewer.h"
#include <iostream>

int main(int argc, char* argv[]) {
    Ultimate64Viewer viewer;
    
    if (!viewer.initialize()) {
        std::cerr << "Failed to initialize Ultimate64Viewer" << std::endl;
        return 1;
    }
    
    // Run the main loop
    viewer.run();
    
    return 0;
}

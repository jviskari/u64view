#include "stream_receiver.h"
#include <iostream>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #define closesocket close
#endif

StreamReceiver::StreamReceiver(const std::string& multicast_group, int port)
    : multicast_group_(multicast_group), port_(port), socket_(-1), running_(false) {
}

StreamReceiver::~StreamReceiver() {
    stop();
}

bool StreamReceiver::setupMulticastSocket() {
    // Create socket
    socket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_ < 0) {
        std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Set socket to non-blocking
    #ifdef _WIN32
        u_long mode = 1;
        ioctlsocket(socket_, FIONBIO, &mode);
    #else
        int flags = fcntl(socket_, F_GETFL, 0);
        fcntl(socket_, F_SETFL, flags | O_NONBLOCK);
    #endif
    
    // Bind to port
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);
    
    if (bind(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "Failed to bind socket: " << strerror(errno) << std::endl;
        closesocket(socket_);
        socket_ = -1;
        return false;
    }
    
    // Join multicast group
    ip_mreq mreq = {};
    mreq.imr_multiaddr.s_addr = inet_addr(multicast_group_.c_str());
    mreq.imr_interface.s_addr = INADDR_ANY;
    
    if (setsockopt(socket_, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
                   reinterpret_cast<const char*>(&mreq), sizeof(mreq)) < 0) {
        std::cerr << "Failed to join multicast group: " << strerror(errno) << std::endl;
        closesocket(socket_);
        socket_ = -1;
        return false;
    }
    
    return true;
}

void StreamReceiver::start() {
    if (running_.load()) return;
    
    running_ = true;
    receiver_thread_ = std::thread(&StreamReceiver::receiveLoop, this);
}

void StreamReceiver::stop() {
    if (!running_.load()) return;
    
    running_ = false;
    
    if (receiver_thread_.joinable()) {
        receiver_thread_.join();
    }
    
    if (socket_ >= 0) {
        closesocket(socket_);
        socket_ = -1;
    }
}

void StreamReceiver::receiveLoop() {
    char buffer[1024];
    
    while (running_.load()) {
        ssize_t bytes_received = recv(socket_, buffer, sizeof(buffer), 0);
        
        if (bytes_received > 0) {
            processPacket(buffer, bytes_received);
        } else if (bytes_received < 0) {
            #ifdef _WIN32
                int error = WSAGetLastError();
                if (error != WSAEWOULDBLOCK) {
                    std::cerr << "Receive error: " << error << std::endl;
                    break;
                }
            #else
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    std::cerr << "Receive error: " << strerror(errno) << std::endl;
                    break;
                }
            #endif
        }
        
        // Small delay to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}
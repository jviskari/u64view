#include "stream_receiver.h"
#include "config.h"
#include <iostream>
#include <cstring>
#include <chrono>

StreamReceiver::StreamReceiver(const std::string& group, int port)
    : sock(INVALID_SOCKET_VALUE), multicast_group(group), port(port), running(false) {
}

StreamReceiver::~StreamReceiver() {
    stop();
}

void StreamReceiver::start() {
    if (!running && sock != INVALID_SOCKET_VALUE) {
        running = true;
        receiver_thread = std::thread(&StreamReceiver::receiverLoop, this);
    }
}

void StreamReceiver::stop() {
    running = false;
    if (receiver_thread.joinable()) {
        receiver_thread.join();
    }
    if (sock != INVALID_SOCKET_VALUE) {
        close_socket(sock);
        sock = INVALID_SOCKET_VALUE;
    }
}

bool StreamReceiver::setupMulticastSocket() {
    // Create socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET_VALUE) {
        std::cerr << "Socket creation failed for port " << port << std::endl;
        return false;
    }
    
    // Set socket to non-blocking
#ifdef _WIN32
    u_long mode = 1;
    if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
        std::cerr << "Failed to set non-blocking mode" << std::endl;
        return false;
    }
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::cerr << "Failed to set non-blocking mode" << std::endl;
        return false;
    }
#endif
    
    // Allow address reuse
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 
                   reinterpret_cast<const char*>(&reuse), sizeof(reuse)) == SOCKET_ERROR_VALUE) {
        std::cerr << "Failed to set SO_REUSEADDR" << std::endl;
        return false;
    }
    
    // Bind socket
    sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR_VALUE) {
        std::cerr << "Bind failed for port " << port << ": " << strerror(errno) << std::endl;
        return false;
    }
    
    // Join multicast group
    ip_mreq mreq = {};
    mreq.imr_multiaddr.s_addr = inet_addr(multicast_group.c_str());
    mreq.imr_interface.s_addr = INADDR_ANY;
    
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
                   reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == SOCKET_ERROR_VALUE) {
        std::cerr << "Failed to join multicast group " << multicast_group 
                  << " on port " << port << ": " << strerror(errno) << std::endl;
        return false;
    }
    
    std::cout << "Successfully joined multicast group " << multicast_group 
              << " on port " << port << std::endl;
    return true;
}

void StreamReceiver::receiverLoop() {
    char buffer[Config::Network::MAX_PACKET_SIZE];
    
    while (running) {
        ssize_t bytes_received = recv(sock, buffer, sizeof(buffer), 0);
        
        if (bytes_received == SOCKET_ERROR_VALUE) {
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
#endif
                std::this_thread::sleep_for(std::chrono::milliseconds(Config::Network::SOCKET_TIMEOUT_MS));
                continue;
            }
            std::cerr << "recv failed on port " << port << ": " << strerror(errno) << std::endl;
            break;
        }
        
        if (bytes_received > 0) {
            processPacket(buffer, bytes_received);
        }
    }
}
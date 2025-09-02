#ifndef STREAM_RECEIVER_H
#define STREAM_RECEIVER_H

#include <atomic>
#include <thread>
#include <string>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #define SOCKET_TYPE SOCKET
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
    #define close_socket closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #define SOCKET_TYPE int
    #define INVALID_SOCKET_VALUE -1
    #define SOCKET_ERROR_VALUE -1
    #define close_socket close
#endif

class StreamReceiver {
protected:
    SOCKET_TYPE sock;
    std::string multicast_group;
    int port;
    std::atomic<bool> running;
    std::thread receiver_thread;
    
public:
    StreamReceiver(const std::string& group, int port);
    virtual ~StreamReceiver();
    
    virtual bool initialize() = 0;
    virtual void start();
    virtual void stop();
    
protected:
    virtual void processPacket(const char* data, size_t size) = 0;
    bool setupMulticastSocket();
    void receiverLoop();
};

#endif // STREAM_RECEIVER_H
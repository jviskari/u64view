#ifndef STREAM_RECEIVER_H
#define STREAM_RECEIVER_H

#include <string>
#include <thread>
#include <atomic>

class StreamReceiver {
protected:
    std::string multicast_group_;
    int port_;
    int socket_;
    std::atomic<bool> running_;
    std::thread receiver_thread_;
    
public:
    StreamReceiver(const std::string& multicast_group, int port);
    virtual ~StreamReceiver();
    
    virtual bool initialize() = 0;
    virtual void start();
    virtual void stop();
    
protected:
    bool setupMulticastSocket();
    virtual void processPacket(const char* data, size_t size) = 0;
    
private:
    void receiveLoop();
};

#endif // STREAM_RECEIVER_H
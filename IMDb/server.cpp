#include <iostream>
#include "server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#include <sys/event.h>
#include <unordered_map>

static void setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        std::cerr << "fcntl F_GETFL failed: " << strerror(errno) << "\n";
        return;
    }
    if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::cerr << "fcntl F_SETFL failed: " << strerror(errno) << "\n";
    }
}

struct Client
{
    int fd;
    std::string readBuffer;
    std::string writeBuffer;
};

static std::unordered_map<int, Client> clients;

void runServer()
{
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);

    
    if (serverFd == -1) {
        std::cerr << "failed to create socket: " << strerror(errno) << "\n";
        return;
    }
    
    std::cout << "Socket created: " << serverFd << "\n";
    
    int opt = 1;
    if(setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        std::cerr << "Failed to set SO_REUSEADDR" << strerror(errno) << "\n";
        close(serverFd);
        return;
    }

    std::cout << "SO_REUSEADDR set" << std::endl;
    
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(6379);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if((bind(serverFd, (struct sockaddr*)&addr, sizeof(addr))) == -1) {
        std::cerr << "Failed to bind: " << strerror(errno) << "\n";
        close(serverFd);
        return;
    }
    
    std::cout << "Bound to 0.0.0.0:6379" << std::endl;
    
    if(listen(serverFd, 128) == -1) {
        std::cerr << "Failed to listen: " << strerror(errno) << "\n";
        close(serverFd);
        return;
    }
    
    std::cout << "Listening on port 6379" << "\n";
    
    setNonBlocking(serverFd);
    std::cout << "Server socket set to non-blocking\n";
    
    int kq = kqueue();
    if (kq == -1) {
        std::cerr << "Failed to create kqueue: " << strerror(errno) << "\n";
        close(serverFd);
        return;
    }
    std::cout << "kqueue created: " << kq << "\n";
    
    struct kevent change;
    EV_SET(&change, serverFd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
    if (kevent(kq, &change, 1, nullptr, 0, nullptr) == -1) {
        std::cerr << "Failed to register server socket with kqueue: " << strerror(errno) << "\n";
        close(serverFd);
        close(kq);
        return;
    }
    std::cout << "Server socket registerd with kqueue\n";
    
    std::cout << "Waiting for connections..." << std::endl;
    
    struct kevent events[64];

    while (true) {
        struct timespec timeout;
        timeout.tv_sec = 0;
        timeout.tv_nsec = 100 * 1000 * 1000;
        
        int numberOfEvents = kevent(kq, nullptr, 0, events, 64, &timeout);
        
        if(numberOfEvents == -1) {
            std::cerr << "kevent wait failed: " << strerror(errno) << "\n";
            break;
        }
        
        for (int i = 0; i < numberOfEvents; i++) {
            int fd = (int)events[i].ident;
            
            if (fd == serverFd) {
                struct sockaddr_in clientAddress;
                socklen_t clientLength = sizeof(clientAddress);
                int clientFd = accept(serverFd, (struct sockaddr*)&clientAddress, &clientLength);
                
                if (clientFd == -1) {
                    std::cerr << "accept failed: " << strerror(errno) << "\n";
                    continue;
                }
                
                setNonBlocking(clientFd);
                
                struct kevent clientEvent;
                EV_SET(&clientEvent, clientFd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
                if(kevent(kq, &clientEvent, 1, nullptr, 0, nullptr) == -1) {
                    std::cerr << "failed to register client fd: " << strerror(errno) << "\n";
                    close(clientFd);
                    continue;
                }
                std::cout << "Client connected. fd=" << clientFd << "\n";
                clients[clientFd] = Client{clientFd, "", ""};
            } else {
                auto it = clients.find(fd);
                if (it == clients.end()) continue;
                Client& client = it->second;
                
                char buf[4096];
                ssize_t bytesRead = read(fd, buf, sizeof(buf));
                
                if (bytesRead == 0 || (bytesRead == -1 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                    std::cout << "Client disconnected. fd=" << fd << "\n";
                    
                    struct kevent removeEvent;
                    EV_SET(&removeEvent, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
                    kevent(kq, &removeEvent, 1, nullptr, 0, nullptr);
                    
                    close(fd);
                    clients.erase(fd);
                    continue;
                }
                
                if (bytesRead > 0) {
                    client.readBuffer.append(buf, bytesRead);
                    client.writeBuffer += client.readBuffer;
                    client.readBuffer.clear();
                }
                
                if (!client.writeBuffer.empty()) {
                    ssize_t bytesWritten = write(fd, client.writeBuffer.data(), client.writeBuffer.size());
                    if (bytesWritten > 0) {
                        client.writeBuffer.erase(0, bytesWritten);
                    }
                }
            }
        }
    }
}



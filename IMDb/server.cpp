#include <iostream>
#include "server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

void runServer() {
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
    
    std::cout << "Waiting for connections..." << std::endl;

    while (true) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        
        int clientFd = accept(serverFd, (struct sockaddr*)&clientAddr, &clientLen);
        
        if (clientFd == -1) {
            std::cerr << "Failed to accept: " << strerror(errno) << std::endl;
            continue;
        }
        
        std::cout << "Client connected. fd=" << clientFd << std::endl;
        
        char buffer[4096];
        
        while (true) {
            ssize_t bytesRead = read(clientFd, buffer, sizeof(buffer));
            
            if (bytesRead <= 0) {
                std::cout << "Client disconnected. fd=" << clientFd << std::endl;
                break;
            }
            write(clientFd, buffer, bytesRead);
        }
        close(clientFd);
    }
}



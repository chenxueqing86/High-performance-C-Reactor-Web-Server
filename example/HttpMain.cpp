#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "TcpConnection.h"
#include "Channel.h"
#include "AsyncLogging.h"
#include "Logging.h"
#include <iostream>
#include <memory>
#include <unordered_map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <netinet/tcp.h>
#include <errno.h>

int createListenFd(int port){
    int fd = socket(AF_INET,SOCK_STREAM,0);
    int opt = 1;
    setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if(bind(fd,(struct sockaddr*)&addr,sizeof(addr)) < 0) return -1;
    if(listen(fd,128) < 0) return -1; //经典的监听失败
    fcntl(fd,F_SETFL,fcntl(fd,F_GETFL) | O_NONBLOCK);
    return fd;
}

int main(){
    AsyncLogging::getInstance("http_server.log",3)->start();

    int port = 8889;
    EventLoop mainLoop;
    EventLoopThreadPool pool(&mainLoop);
    pool.setThreadNum(4);
    pool.start();

    int listenFd = createListenFd(port);
    LOG_INFO("HTTP server listening on port %d",port);

    Channel listenChannel(&mainLoop,listenFd);
    listenChannel.setReadCallback([&](){
        while(true){
            struct sockaddr_in client;
            socklen_t len = sizeof(client);
            int connFd = accept(listenFd,(struct sockaddr*)&client,&len);
            if(connFd < 0){
                if(errno == EAGAIN || errno == EWOULDBLOCK) break;
                else break;
            }
            fcntl(connFd,F_SETFL,fcntl(connFd,F_GETFL) | O_NONBLOCK);
            int flag = 1;
            setsockopt(connFd,IPPROTO_TCP,TCP_NODELAY,&flag,sizeof(flag));

            EventLoop* ioLoop = pool.getNextLoop();
            ioLoop->runInLoop ([connFd,ioLoop](){
                static thread_local std::unordered_map<int,std::shared_ptr<TcpConnection
                >> connections;
                auto conn = std::make_shared<TcpConnection>(ioLoop,connFd,false); //false = HTTP 模式
                connections[connFd] = conn;
                conn->setCloseCallback([connFd](int) {
                    connections.erase(connFd);
                });
            });
        }
    });
    listenChannel.enableReading();

    mainLoop.loop();
    return 0;
}
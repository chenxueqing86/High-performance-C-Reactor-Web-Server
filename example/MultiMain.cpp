#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "TcpConnection.h"
#include "Channel.h"
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
    if(fd < 0) return -1;

    int opt = 1;
    setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if(bind(fd,(struct sockaddr*)&addr,sizeof(addr)) < 0){
        close(fd);
        return -1;
    }
    if(listen(fd,128) < 0){
        close(fd);
        return -1;
    }
    fcntl(fd,F_SETFL,fcntl(fd,F_GETFL)| O_NONBLOCK);  
    return fd;
}

int main(int argc,char* argv[]){
   int port = 8889;  //改用8889作为HTTP端口(聊天室)

    if(argc > 1) port = atoi(argv[1]);

    // 主 EventLoop (mainReactor)
    EventLoop mainLoop;

    //线程池：4 个 IO线程 （可根据 CPU 核心数调整）
    EventLoopThreadPool pool(&mainLoop);
    pool.setThreadNum(4);
    pool.start();

    int listenFd = createListenFd(port);
    if(listenFd < 0){
        std::cerr << "create listen fd failed" << std::endl;
        return -1;
    }
    std::cout << "Multi-threaded server listening on port" << port
          << " (mainReactor + 4 IO threads)" << std::endl;
    
    //监听 Channel (在主线程处理)
    Channel listenChannel(&mainLoop,listenFd);
    listenChannel.setReadCallback([&]() {
        while(true){
            struct sockaddr_in client;
            socklen_t len = sizeof(client);
            int connFd = accept(listenFd,(struct sockaddr*)&client,&len);
            if(connFd < 0){
                if(errno == EAGAIN || errno == EWOULDBLOCK) break;
                else{
                //    perror("accept");
                    break;
                }
            }
            //设置非阻塞 + TCP_NODELAY
            fcntl(connFd,F_SETFL,fcntl(connFd,F_GETFL) | O_NONBLOCK);
            int flag = 1;
            setsockopt(connFd,IPPROTO_TCP,TCP_NODELAY,&flag,sizeof(flag));

            //轮询选择一个IO线程
            EventLoop* ioLoop = pool.getNextLoop();

            //在 IO 线程中创建 TcpConnection
            ioLoop->runInLoop([connFd,ioLoop]()  {
                // thread_Local 连接容器（每个 IO 线程独立）
                static thread_local std::unordered_map<int,std::shared_ptr<TcpConnection>> connections;
           //     auto conn = std::make_shared<TcpConnection>(ioLoop,connFd,true /* 使用长度头协议 */);
                 auto conn = std::make_shared<TcpConnection>(ioLoop,connFd,false /* 使用长度头协议 */);
                connections[connFd] = conn;

                //消息回调：回显
                // conn->setMessageCallback([](std::shared_ptr<TcpConnection> conn,const char* data,size_t len){
                //     conn->send(data,len);
                // });

                // 关闭回调：从连接容器中移除
                conn->setCloseCallback([connFd](int /*fd*/){
                    // 注意：closeCallback 在IO 线程执行，所以可以访问thread_Local 的 connections
                    connections.erase(connFd);
     //               std::cout << "Connection" << connFd << "closed and removed" << std::endl;
                });
              
             //std::cout << "New connection " << connFd
              //          << " assigned to IO thread " << std::this_thread::get_id()  <<std::endl;
        //   }); 
       
    });
}
    });
    listenChannel.enableReading();

    //进入主事件循环（阻塞）
    mainLoop.loop();

    return 0;
}
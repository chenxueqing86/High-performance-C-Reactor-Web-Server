/*2
#include "EventLoop.h"
#include "Channel.h"
#include "TcpConnection.h"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <chrono>
//day5，关于TCP_NODELAY 的头文件
#include <netinet/tcp.h>

int createListenFd(int port){
    int fd = socket(AF_INET,SOCK_STREAM,0);
    if(fd < 0) {
        perror("socket");
        return -1;
    }

    int opt = 1;
   if (setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt)) < 0){
        perror("setsocketopt");
        close(fd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if(bind(fd,(struct sockaddr*)&addr,sizeof(addr)) < 0){
        perror("bind");
        close(fd);
        return -1;
    }
    if(listen(fd,10) < 0){
        perror("listen");
        close(fd);
        return -1;
    }
    //设置为非阻塞
   if(fcntl(fd,F_SETFL,fcntl(fd,F_GETFL) | O_NONBLOCK) < 0){
    perror("fcntl");
    close(fd);
    return -1;
   }
    return fd;
}
2 */
/*int main(){
    EventLoop loop;*/

  /*  int listenFd = createListenFd();
    if(listenFd < 0){
        std::cerr << "create listen fd failed" << std::endl;
        return -1;
    }
    std::cout << "Server listening on port 8888" << std::endl;*/

    /* 2
     //day5,监听端口8888（长度头协议）
     int echoFd = createListenFd(8888);
     if(echoFd < 0){
        std::cerr << "create echo listen fd failed" << std::endl;
        return -1;
     }
     std::cout << "Echo server (length-header) on port 8888" << std::endl;

     //监听端口 8889（HTTP）
     int httpFd = createListenFd(8889);
     if(httpFd < 0) {
        std::cerr << "create HTTP listen fd failed" << std::endl;
        return -1;
     }
     std::cout << "HTTP server on port 8889" << std::endl;
     //二天新添加，没有顺序的键值对
    std::unordered_map<int,std::shared_ptr<TcpConnection>> connections;
    
    auto acceptCallback = [&](int listenFd,bool useLengthHeader){
        while(true){
            struct sockaddr_in client;
            socklen_t len = sizeof(client);
            int connFd = accept(listenFd,(struct sockaddr*)&client,&len);
            if(connFd < 0){
                if(errno == EAGAIN || errno == EWOULDBLOCK) break;
                else { std::cerr << "accept error"  << std::endl; break;}
            }
            //day5:降低压测的时间（两句）
            int flag = 1;
            setsockopt(connFd,IPPROTO_TCP,TCP_NODELAY,&flag,sizeof(flag));

            fcntl(connFd,F_SETFL,fcntl(connFd,F_GETFL) | O_NONBLOCK);
            std::cout << "New connection from" << inet_ntoa(client.sin_addr)
            << ":" << ntohs(client.sin_port)
            << (useLengthHeader ? " (echo)": " (HTTP)") << std::endl;
            
        auto conn = std::make_shared<TcpConnection>(&loop,connFd,useLengthHeader);
        conn->setCloseCallback([&connections](int fd){
            connections.erase(fd);
            std::cout << "Connection " << fd << " closed "<< std::endl;
        });
        if(useLengthHeader){
            //Echo 回显
            conn->setMessageCallback([](std::shared_ptr<TcpConnection> conn,const char* data,size_t len){
                conn->send(data,len);
            });
        }else{
            //HTTP 不需要额外的消息回显，解析在 parseHttp 中完成
            //但我们需要咋解析完成后自动关闭连接，已经在 parseHttp 中 close
            // 这里可以留空
        }
        connections[connFd] = conn;
      }
    };
     2 */
    //创建监听 Channel
   /* Channel listenChannel(&loop,listenFd);
    listenChannel.setReadCallback([&](){
        while(true){
            struct sockaddr_in client;
            socklen_t len = sizeof(client);
            int connFd = accept(listenFd,(struct sockaddr*)&client,&len);
            if(connFd < 0){
                if(errno  == EAGAIN || errno == EWOULDBLOCK) break;
                else { std::cerr << "accept error" << strerror(errno) << std::endl; break;}
            }
        //设为非阻塞
        fcntl(connFd,F_SETFL,fcntl(connFd,F_GETFL) | O_NONBLOCK);
        std::cout << "New connection from "
                  << inet_ntoa(client.sin_addr) << ":"
                  << ntohs(client.sin_port) << std::endl;*/

       /* //为每个连接创建Channel (简单处理，实际应用应使用只能指针管理)
        Channel* connChannel = new Channel(&loop,connFd);
        connChannel->setReadCallback([connChannel,connFd](){
            char buf[1024];
            while(true){
                ssize_t n = recv(connFd,buf,sizeof(buf),0);
                if(n < 0){
                    if(errno == EAGAIN || errno == EWOULDBLOCK) break;
                    else{
                        std::cerr << "recv error,closing" << std::endl;
                        close(connFd);
                        delete connChannel;
                        return;
                    }
                }else if(n == 0){
                    //客户端关闭
                    std::cout << "Connection closed" << std::endl;
                    close(connFd);
                    delete connChannel;
                    return;
                }else{
                    //回显
                    send(connFd,buf,n,0);
                }
            }
        });
        //启用读事件
        connChannel->enableReading();*/

        //day2,创建 TcpConnection 管理连接
   /* auto conn = std::make_shared<TcpConnection>(&loop,connFd);
    conn->setCloseCallback([&connections](int fd){
        auto it = connections.find(fd);
        if(it != connections.end()){
            connections.erase(fd);
        }
        std::cout << "Connection" << fd << "closed and removed from map" << std::endl;
    });
    //设置消息回调：回显（Echo）
    conn->setMessageCallback([](std::shared_ptr<TcpConnection> conn,const char* data,size_t len){
        std::cout << "Received complete message: " << std::string(data,len) <<std::endl;
        //回显
        conn->send(data,len);
    });
    connections[connFd] = conn;
    // 连接关闭时从 map移除，需要设置一个关闭回调
     // 这里简化为在 TcpConnection 析构时自动移除，但需要额外机制
     // 为了简单，我们不在 TcpConnection 中回调，而是在 main 中主动检查？
     // 实际更规范的做法是设置 TcpConnection 的 closeCallback。
     // 这里为了演示，我们保留智能指针，连接关闭时 map 自动清理？
     // 不，需要显式 erase，否则 map 会保留悬空 shared_ptr。
    // 我们在 TcpConnection 中增加一个 closeCallback，在关闭时调用。
        }
    }); */

    
   /*listenChannel.enableReading();
    //day4，添加一个每3秒打印一次的定时器，并演示runAfter
    //每三秒打印一次
    loop.runEvery(std::chrono::milliseconds(2000),[](){
      //  std::cout << "Timer tick at " << time(nullptr) << std::endl;
      std::cout << "Every 2s timer fired" << std::endl;
    });

    //延迟5秒后执行一次
    loop.runAfter(std::chrono::milliseconds(5000),[](){
        // std::cout << "One-shot timer fired" << std::endl;
          std::cout << "One-shot timer fired after 5s" << std::endl;
    });*/ 
  
    //day5,创建两个监听 Channel
    /*  2
    Channel echoChannel(&loop,echoFd);
    echoChannel.setReadCallback([&]() { acceptCallback(echoFd,true); });
    echoChannel.enableReading();

    Channel httpChannel(&loop,httpFd);
    httpChannel.setReadCallback([&]() { acceptCallback(httpFd,false); });
    httpChannel.enableReading();
    //进入事件循环
    loop.loop();
    return 0;
}
*/
#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "TcpConnection.h"
#include "Channel.h"
#include <iostream>
#include <memory>
#include <vector>
#include <mutex>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <netinet/tcp.h>
#include <errno.h>
//day10
#include "Logging.h"

// 全局连接列表 （线程安全）
std::vector<std::shared_ptr<TcpConnection>> g_connections;
std::mutex g_connMutex;

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
    if(bind(fd,(struct sockaddr*)&addr,sizeof(addr)) < 0) { close(fd); return -1; }
    if(listen(fd,128) < 0) { close(fd);  return -1; }
    fcntl(fd,F_SETFL,fcntl(fd,F_GETFL) | O_NONBLOCK);
    return fd;
}

// 广播消息给所有连接
void broadcast(const std::string& msg,const std::shared_ptr<TcpConnection>& sender){
   // std::cout << "Broadcasting  to " << conns.size() << " connections" << std::endl;
    // 先在所内拷贝一份列表，避免长时间持锁
    std::vector<std::shared_ptr<TcpConnection>> conns;
   {
    std::lock_guard<std::mutex> lock(g_connMutex);
    conns = g_connections;
   }
  // std::cout << "[Broadcast] msg=\"" << msg << "\" to " << conns.size()
   //        << " connections (sender fd= "  << sender->fd() << ")" << std::endl;  //新增

   //day10
    LOG_INFO("[Broadcast] msg=\"%s \" to %zu connections (sender fd= %d)",msg.c_str(),conns.size(),sender->fd());        
   for(auto& conn :conns){
    if(conn && !conn->closed()){
        conn->send(msg);  // send() 内部处理跨线程投递
    }
   }
}



int main(int argc,char* argv[]){
    //day10: 启动异步日志（3秒超时刷盘）
    AsyncLogging::getInstance("chat_server.log",3)->start();

    int port = 8890;
    if(argc > 1) port = atoi(argv[1]);

    EventLoop mainLoop;
    EventLoopThreadPool pool(&mainLoop);
    pool.setThreadNum(4);
    pool.start();

    int listenFd = createListenFd(port);
    if(listenFd < 0){
        std::cerr << "create listen fd failed" << std::endl;
        return -1;
    }
  //  std::cout << "Chat server on port " << port
    //         << " (mianReactor + 4 IO threads)"  << std::endl;
    LOG_INFO("Chat server on  %d  (mianReactor + 4 IO threads)",port);  // 用printf 风格格式化
    Channel listenChannel(&mainLoop,listenFd);
    listenChannel.setReadCallback([&]() {
        while(true){
            struct sockaddr_in client;
            socklen_t len = sizeof(client);
            int connFd = accept(listenFd,(struct sockaddr*)&client,&len);
            if(connFd < 0){
                if(errno == EAGAIN || EWOULDBLOCK) break;
                else break;
            }
            fcntl(connFd,F_SETFL,fcntl(connFd,F_GETFL) | O_NONBLOCK);
            int flag = 1;
            setsockopt(connFd,IPPROTO_TCP,TCP_NODELAY,&flag,sizeof(flag));

            EventLoop* ioLoop = pool.getNextLoop();
            ioLoop->runInLoop([connFd,ioLoop,&client](){
                //使用长度头协议
                auto conn = std::make_shared<TcpConnection>(ioLoop,connFd,true);

                // 加入全局连接列表
                {
                    std::lock_guard<std::mutex> lock(g_connMutex);
                    g_connections.push_back(conn);
                }

                // 消息回调
                conn->setMessageCallback([](std::shared_ptr<TcpConnection> self,
                                         const char* data,size_t len) {
                    std::string msg(data,len);
                 //   std::cout << " [Chat] message from fd " << self->fd()
                   //         << ": " << msg << std::endl;
                            LOG_INFO(" [Chat] message from %d: %s",self->fd(),msg.c_str());
                     //广播给所有连接 （包括发送者自己）
                    broadcast(msg,self);
              });

              // 关闭回调：从全局列表中移除
              conn->setCloseCallback([connFd](int){
                std::lock_guard<std::mutex> lock(g_connMutex);
                g_connections.erase(
                    std::remove_if(g_connections.begin(),g_connections.end(),
                    [connFd](const std::shared_ptr<TcpConnection>& c) {
                        return !c || c->fd() == connFd;
                    }),
                    g_connections.end());
                 //   std::cout << "[Chat] connection " << connFd << " removed." << std::endl;
                 LOG_INFO("[Chat] connection  %d removed.",connFd);
              });

              std::cout << "New chat client " << connFd 
                         << " assigned to IO thread " << std::this_thread::get_id() << std::endl;
            });
        }
    });
    listenChannel.enableReading();

    mainLoop.loop();
    return 0;
}


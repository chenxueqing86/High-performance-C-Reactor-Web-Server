#include <iostream>
#include <cstring>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <arpa/inet.h>  //htonl,ntohl,htons,inet_pton
#include <sys/socket.h> // socket,connect,send,recv,MSG_WATALL
#include <netinet/in.h>  // sockaddr_in,AF_INET,SOCK_STREAM

const int PORT = 8890;
const char* SERVER_IP = "127.0.0.1";

//发送带长度头的消息
bool sendMsg(int sock,const std::string& msg){
    uint32_t netLen = htonl(msg.size());
   // if(send(sock,&netLen,4,0) != 4) return false;
    // if(send(sock,msg.c_str(),msg.size(),0) != (ssize_t)msg.size())  return false;
    send(sock,&netLen,4,0);
    send(sock,msg.c_str(),msg.size(),0);
    return true;
}

// 接收消息（阻塞）
void recvLoop(int sock,std::atomic<bool>& running){
    std::cout << "recvLoop started"  << std::endl;  //新增
    while(running){
        uint32_t netLen;
        // ssize_t n = recv(sock,&netLen,4,MSG_WAITALL);
        // //检测
        // std::cout << "recv header: n =" << n << std::endl; //新增

        // if(n <= 0) break;
        /* 读取 4 字节头部*/
        size_t headerRead = 0;
        while(headerRead < 4){
            ssize_t n = recv(sock,(char*)&netLen + headerRead,4 - headerRead,0);
            if(n <= 0) { running = false; return; }
            headerRead += n;
        }
        uint32_t len = ntohl(netLen);
        if(len > 65536)  break;
        std::string buf(len,'\0');
        size_t received = 0;
        while(received < len){
            ssize_t n = recv(sock,&buf[received],len - received,0);
            if(n <= 0) { running = false; return;  }
            received += n;
        }
        std::cout << "\n[Received] " << buf << std::endl;
        std::cout << "> " << std::flush;
     }
}

int main(){
    int sock = socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET,SERVER_IP,&addr.sin_addr);

    if(connect(sock,(struct sockaddr*)&addr,sizeof(addr)) < 0){
        std::cerr << "connect failed" << std::endl;
        return -1;
    }
    std::cout << "Connected to chat server. Type message and press enter.\n" << std::flush;

    std::atomic<bool> running(true);
    std::thread recvThread(recvLoop,sock,std::ref(running));

    std::string line;
    while(running && std::getline(std::cin,line)){
        if(line == "quit") break;
        if(!sendMsg(sock,line)) break;
        std::cout << "> " << std::flush;
    }

    running = false;
    close(sock);
    recvThread.detach();
    return 0;
}
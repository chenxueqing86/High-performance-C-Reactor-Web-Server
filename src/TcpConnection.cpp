#ifdef DEBUG
#define LOG(msg) std::cout<< msg << std::endl
#else
#define LOG(msg)  ((void)0)
#endif

#include "TcpConnection.h"
#include "EventLoop.h"
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <cstring>
#include <cerrno>
#include <arpa/inet.h> //提供 htonl,ntohl
#include <memory>
//day10
#include "Logging.h"
//day11,支持静态文件
#include  <sys/stat.h>
#include <sys/sendfile.h> 
#include <fcntl.h>

//day5.增加uselenghHeader
TcpConnection::TcpConnection(EventLoop* loop,int sockfd,bool useLengthHeader)
           :loop_(loop),sockfd_(sockfd),
           channel_(std::make_unique<Channel>(loop,sockfd)),
           writing_(false),
           useLengthHeader_(useLengthHeader),
           closeAfterWrite_(false),
           closed_(false),
           keepAlive_(true),contentLength_(0){
            //day5 error3:有残留
            inputBuffer_.retrieveAll();
            outputBuffer_.retrieveAll();
            //设置回溯
            channel_->setReadCallback([this]() { handleRead(); });
            channel_->setWriteCallback([this]() { handleWrite();});
            channel_->setErrorCallback([this]() { handleError();});
            //初始只关注读事件
            channel_->enableReading();
            //增加判断
            
}

 
TcpConnection::~TcpConnection(){
    // 关闭 socket （如果尚未关闭）
    // if(sockfd_ > 0)
    // {
    //     ::close(sockfd_);
    // }
    // std::cout << "TcpConnection destructed,fd= " << sockfd_ << std::endl;
    //此时已经在 IO 线程 （由 closeInLoop 出发删除）,直接关闭fd即可
    if(!closed_){
        ::close(sockfd_);
    }
}

/*void TcpConnection::send(const char* data,size_t len){
    //day5 error4:打印发现错误
    std::cout << "send() called with " << len << " bytes: [" << std::string(data,len) << "]" <<std::endl;
    outputBuffer_.append(data,len);
    //day5 error3:打印要发送的数据
    std::cout << "Sending response: [" << std::string(data,len) << "]" << std::endl;
 //   if(len == 0 ) return;
    //将数据追加到输出缓冲区
    //day3.添加长度头（4字节网络序）
    uint32_t netLen = htonl(len);
    outputBuffer_.append(reinterpret_cast<const char*>(&netLen),4);
   outputBuffer_.append(data,len);

  
    //如果当前没有再写，尝试直接发送
    if( !writing_){
        //直接发送，如果写不完则启用写事件
        ssize_t n = ::send(sockfd_,outputBuffer_.peek(),outputBuffer_.readableBytes(),0);
        if(n < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                // 写缓冲区满，需要等待写事件
                writing_ = true;
                channel_->enableWriting();
                return;
            }else{
                //错误，关闭连接
                close();
                return;
            }
        }
        outputBuffer_.retrieve(n);  //// 解析完数据，从缓冲区移除n字节
        if(outputBuffer_.readableBytes() > 0){
            writing_ = true;
            channel_->enableWriting();
        }
    }else{
        //已经再写，由写事件驱动
        channel_->enableWriting();
    }
}*/
/*第一阶段版本：线程安全的 send*/
void TcpConnection::send(const char* data,size_t len){
    if(closed_) return;
    
    std::string msg;
    if(useLengthHeader_){
        //拼装协议包 ；4字节长度头 + 数据体
        uint32_t netLen = htonl(len);
        msg.append(reinterpret_cast<const char*>(&netLen),4);
        msg.append(data,len);
    }else{
        // HTTP 模式或者其他模式，不加长度头
        msg.assign(data,len);
    }

    //投递到 IO 线程
    if(loop_ ->isInLoopThread()){
       // sendInLoop(data,len);
       sendInLoop(msg.c_str(),msg.size());
    }else{
        auto self = shared_from_this();
     //   std::string msg(data,len);
        loop_ ->runInLoop([self,msg](){
            self->sendInLoop(msg.c_str(),msg.size());
        });
    }
}

void TcpConnection::send(const std::string& message){
    send(message.c_str(),message.size());
}

void TcpConnection::sendInLoop(const char* data,size_t len){
    //std::cout << "sendInLoop: len = " << len << "data=" << std::string(data,len) << std::endl;
    //day10
    LOG_INFO("sendInLoop: len = %d data=%s",len,std::string(data,len).c_str());
    if(closed_) return;
    outputBuffer_.append(data,len);
    if(!writing_){
        ssize_t n = ::send(sockfd_,outputBuffer_.peek(),outputBuffer_.readableBytes(),0);
        if(n < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                writing_ = true;
                channel_->enableWriting();
                return;
            }else{
                close();
                return;
            }
        }
        outputBuffer_.retrieve(n);
        if(outputBuffer_.readableBytes() > 0){
            writing_ = true;
            channel_->enableWriting();
        }
    }else{
        close();
        return;
    }
}

// void TcpConnection::close(){
//  //  if(sockfd_ <= 0) return;//已经关闭

//     // 从EventLoop 中移除 Channel
//     loop_->removeChannel(channel_.get());
//     //关闭 sockfd,析构时会 close
//     ::close(sockfd_);
//    // sockfd_ = -1;

//     //调用关闭回调（通知上层）
//     if(closeCallback_){
//         closeCallback_(sockfd_); // 注意；这里传递的是旧的 sockfd,单已经关闭，可能用于标识
//     }
// }

// ------------------线程安全的 close ------------------
void TcpConnection::close(){
    if(closed_) return; //已在关闭流程中
    if(loop_ ->isInLoopThread()){
        closeInLoop();
    }else{
        auto self = shared_from_this();
        loop_->runInLoop([self](){
            self->closeInLoop();
        });
    }
}

void TcpConnection::closeInLoop(){
    if(closed_.exchange(true)) return;  //双重检查

    //1.从epoll 移除 Channel
    channel_ ->disableAll();
    loop_ ->removeChannel(channel_.get());

    //2.关闭 socket
    if(sockfd_ >= 0){
        ::close(sockfd_);
        sockfd_ = -1;
    }

    //3.通知上层（此时仍在 IO 线程）
    if(closeCallback_){
        closeCallback_(channel_->fd());
    }
    //TcpConnection 对象将在connections 容器中 erase后销毁
}

void TcpConnection::handleRead(){
    char buf[65536]; //一次尽量多读
    while(true){
        ssize_t n = ::recv(sockfd_,buf,sizeof(buf),0);
        if(n < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                //数据读完，处理接收到的数据（此处直接回显）
               /* std::string received = inputBuffer_.retrieveAsString(inputBuffer_.readableBytes());
                if(!received.empty()){
                    std::cout << "Received: " << received << std::endl;
                    //回显
                   send(received);
                }*/
                //day3.数据读完解析完整消息
              //  parseAndProcess();
              //day5:修改，根据协议类型解析
              if(useLengthHeader_) parseLengthHeader();
              else parseHttp();
                break;
            }else{
                //读错误，关闭连接
                std::cerr << "recv error: " << strerror(errno) << std::endl;
                close();
                break;
            }
        }else if(n == 0){
            //客户端关闭
            std::cout << "Connection closed by peer" << std::endl;
            close();
            break;
        }else{
            inputBuffer_.append(buf,n);
            //继续循环读取
        }
    }
}

void TcpConnection::handleWrite() {
    if(outputBuffer_.readableBytes() == 0){
        // 写完了，取消写事件
        writing_ = false;
        channel_ ->disableAll();
        channel_ ->enableReading(); //只关注读
        if(closeAfterWrite_){
            close();
        }
        return;
    }
    ssize_t n = ::send(sockfd_,outputBuffer_.peek(),outputBuffer_.readableBytes(),0);
    if(n < 0){
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            //继续等待写事件
            return;
        }else{
            //写错误
            std::cerr << "Send error: " << strerror(errno) << std::endl;
            close();
            return;
        }
    }
    outputBuffer_.retrieve(n);
    if(outputBuffer_.readableBytes() == 0){
        writing_ = false;
        channel_ ->disableAll();
        channel_ ->enableReading();
        if(closeAfterWrite_){
            close();
        }
    }
}
void TcpConnection::handleError(){
    //std::cerr << "TcpConnection error,closing" << std::endl;
    close();
}

//--------------------协议解析----------
//void TcpConnection::parseAndProcess(){
void TcpConnection::parseLengthHeader(){
    while(true){
        //调试
      //  std::cout << "parseLengthHeader: readable = " << inputBuffer_.readableBytes() << std::endl;
      //day10
      LOG_INFO("parseLengthHeader: readable = %zu",inputBuffer_.readableBytes());//`%zu`：C 格式化占位符，专门匹配 `size_t` 类型（无符号整数）
      
        //检查是否有足够数据读取长度头
        if(inputBuffer_.readableBytes() < 4) break;

        //读取长度头（网络序）
        uint32_t netLen;
        memcpy(&netLen,inputBuffer_.peek(),4);
        uint32_t len = ntohl(netLen);
       //调试
       std::cout << " len = " << len << std::endl;
        //检查消息长度合理性（防止恶性攻击）
        if(len > 65536) {
        //    std::cerr << "Invalid message length: " << len << std::endl;
            close();
            return;
        }

        //调试
        if(inputBuffer_.readableBytes() < 4 + len){
            std::cout << " not enough data,need " << (4+len) << ",have " << inputBuffer_.readableBytes() << std::endl;
            break;
        }
        //取出数据（跳过长度头）
        inputBuffer_.retrieve(4);
        std::string message = inputBuffer_.retrieveAsString(len);

        //调用消息回溯
        if(messageCallback_){
            messageCallback_(shared_from_this(),message.c_str(),message.size());
        }
    }
}
/*void TcpConnection::close(){
    loop_->removeChannel(channel_.get());
    ::close(sockfd_);
    if(closeCallback){
        closeCallback_(sockfd_);
    }
}*/

//HTTP 解析（简易版）day5；第二版：为了支持静态文件
void TcpConnection::parseHttp(){

  /* 旧的版本，简易版 */
  // if(inputBuffer_.readableBytes() == 0) return; //新增：防止空数据重复触发
  /*if(inputBuffer_.readableBytes() < 4 ) return; 

 //  std::string request = inputBuffer_.retrieveAsString(inputBuffer_.readableBytes());
  //  inputBuffer_.retrieveAll();
  // std::cout << "Raw request: [" << request << "]" << std::endl;  

   //day5的修改：
   const char* data = inputBuffer_.peek();
   //一部分 1
   //清空缓冲区（确保不会残留）
    // inputBuffer_.retrieveAll();

    // std::cout << "Raw request: [" << request << "]" << std::endl; 
    // if(outputBuffer_.readableBytes() != 0){
    //     std::cerr << "WARNING: outputBuffer_ not empty before sending response! Has "
    //              << outputBuffer_.readableBytes() << " bytes." << std::endl;

    //    //强制清空输出缓冲区（防止残留）
    //     outputBuffer_.retrieveAll();
    // }

    // //只处理 GET / 请求，返回固定内容
    // if(request.find("GET /") == 0){
      //  一部分1 结束

     std::string response;
    //day5最后优化一下：只检查前4个字符是否为“GET”
    //if(inputBuffer_.readableBytes() >= 4 && memcmp(data,"GET",4) == 0){
    if(memcmp(data,"GET ",4) == 0){
       //  static const std::string response = 
       response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: 21\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<h1>Hello,World!</h1>";
     //   inputBuffer_.retrieveAll();
      //  send(response);
    }else{
        //其他请求返回404
     //   inputBuffer_.retrieveAll();
      //  static const std::string notFound = 
      response = 
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Length: 9\r\n"
        "Connection: close\r\n"
        "\r\n"
        "Not Found";
    //   send(notFound);
    }
  //  std::cout << "Sending response: [" << response << "]" << std::endl;
     // 将响应放入输出缓冲区，并尝试发送
   // send(response);

    // 注意：发送后可能数据未完全发送，需等待写完成再关闭
    // 这里我们设置一个标志，在handleWrite中检测
    // 但为了简化，我们可以直接调用close，但可能导致数据丢失。
    // 改为使用shutdown写端，让客户端收到EOF，但更合理的是等待发送完成。
    // 我们添加一个成员变量 closeAfterWrite_，在send后置为true。
    inputBuffer_.retrieveAll();
    closeAfterWrite_ = true;  //先设置，再send（顺序很重要）
     send(response); */
        
    //close(); //短连接，响应后关闭

    //day11 1.简单的 HTTP请求行解析 （真实场景应该用状态机）
    if(inputBuffer_.readableBytes() == 0) return;

    std::string req = inputBuffer_.retrieveAsString(inputBuffer_.readableBytes());
    inputBuffer_.retrieveAll();

     //判断是否保持连接 （HTTP/1.1 默认 keep-alive,除非显示要求 close）
     keepAlive_ = true;
     if(req.find("Connection: close") != std::string::npos){
        keepAlive_ = false;
     }

    std::string response;
    std::string body;

    if(req.find("GET ") == 0){
        body = "<h1>Hello,World!</h1>"; //长度22
        response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/html\r\n";
    }else if(req.find("POST ") == 0){
        // 简单获取POST 的body
        size_t pos = req.find("\r\n\r\n");
        body = (pos != std::string::npos) ? req.substr(pos + 4):"Empty";
        response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/plain\r\n";
    }else{
        body = "Not Found";
        response = "HTTP/1.1 404 Not Found\r\n";
        response += "Content-Type: text/plain\r\n";
        keepAlive_ = false; //404 强制关闭；
    }
  
    
    //统一添加头
    response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    response += keepAlive_ ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
    response += "\r\n"; //空行
    response += body;

    send(response);
    // //解析请求方法和路径
    // bool isGet = (req.find("GET ") == 0);
    // bool isPost = (req.find("POST ") == 0);

    // //默认关闭连接（HTTP/1.0 行为），如果发现 KeepAlive 则打开
    // keepAlive_ = false;
    // if(req.find("Connection: keep-alive") != std::string::npos){
    //     keepAlive_ = true;
    // }


    // if(isGet){
    //     //处理静态文件请求 （这里简单假设访问根目录，返回 Hello）
    //     std::string body = "<h1>Hello,World!</h1>";
    //     response = "HTTP/1.1 200 OK\r\n";
    //     response += "Content-Type: text/html\r\n "; //注意：这里的换行必须是\r\n
    //     response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    //     response += keepAlive_ ? "Connection: keep-alive\r\n " : "Connection: close\r\n";

    //     //检查 keepAlive_ 变量是否生效
    //     if(keepAlive_){
    //         response += "Connection: keep-alive\r\n";
    //     }else{
    //         response += "Connection: close\r\n";
    //     }
        
    //     response += "\r\n";
    //     response += body;

    //     send(response);
    // }
    // else if(isPost){
    //     //处理 POST 请求：把收到的Body 原样返回 （Echo）
    //     //真实场景需要找 \r\n\r\n 提取Body
    //     size_t bodyPos = req.find("\r\n\r\n");
    //     std::string body = (bodyPos != std::string::npos) ? req.substr(bodyPos + 4): "No Body";

    //     response = "HTTP/1.1 200 OK\r\n";
    //     response += "Content-Type: text/plain\r\n";
    //     response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    //     response += keepAlive_ ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
    //     response += "\r\n";
    //     response += body;

    //     send(response);
    // }
    //  else{
    //     std::string body = "Not Found";
    //     response = "HTTP/1.1 404 Not Found\r\n";
    //     response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    //     response += "Connection: close\r\n";
    //     response += "\r\n";
    //     response += body;
    //     send(response);
    //     keepAlive_ = false;
    //  }




     //如果不包吃长连接，则标记发送后关闭
     if(!keepAlive_){
        closeAfterWrite_ = true;
         }
}
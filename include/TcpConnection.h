#pragma once
#include <memory>
#include <functional>
#include <string>
#include "Buffer.h"
#include "Channel.h"
#include <atomic>
//#include "EventLoop.h"
class EventLoop;// 前向声明

class TcpConnection: public std::enable_shared_from_this<TcpConnection>{
public:  
//关闭回调类型：参数为sockfd
    using CloseCallback = std::function<void(int sockfd)>;

    //day3.消息回调：参数 连接共享指针，消息内容，消息长度
    using MessageCallback = std::function<void(std::shared_ptr<TcpConnection>,const char*,size_t)>;
//day5,这里的形参修改了
    TcpConnection(EventLoop* loop,int sockfd,bool useLengthHeader = true);
    ~TcpConnection();

    void send(const std::string& message);
    void send(const char* data,size_t len);
    void close();     //线程安全：自动切换到 IO 线程执行

    // 设置关闭回调（用于通知上层连接已关闭）
    void setCloseCallback(CloseCallback cb) { closeCallback_ = std::move(cb); }
    //day3
    void setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb);}

    //day5 
    int fd() const { return sockfd_; }

    //第一阶段
    bool closed() const { return closed_; }


private:
    void handleRead();
    void handleWrite();
    void handleError();
    //day3.从输入缓冲区解析完整消息，并调用 messageCallback_
 //   void parseAndProcess();

    //day5,http 协议解析
    void parseHttp();
    void parseLengthHeader();

    //第一阶段：只在IO 线程执行
    void closeInLoop();
    void sendInLoop(const char* data,size_t len);  //必须声明 **

    EventLoop* loop_;
    int sockfd_;
    std::unique_ptr<Channel> channel_;

    Buffer inputBuffer_; //输入缓冲区
    Buffer outputBuffer_; //输出缓冲区
    bool writing_; //是否正在写
    //day5:是否使用长度头协议
    bool useLengthHeader_;

    CloseCallback closeCallback_; //连接关闭时调用的回调
    MessageCallback messageCallback_;

    bool closeAfterWrite_; //是否写完关闭

    //第一阶段：防止重复关闭
    std::atomic<bool> closed_;
    //day10
    //用于 Keep-Alive 状态维持
    bool keepAlive_;
    // 用于解析 POST 的body
    size_t contentLength_;    
};
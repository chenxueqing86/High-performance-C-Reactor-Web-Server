#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>

class EventLoop;

class EventLoopThread {
    public:
    EventLoopThread();
    ~EventLoopThread();

    //启动线程，返回该线程上的 EventLoop 指针 (主线程可用其投递任务)
    EventLoop* startLoop();
    
    private:
    void threadFunc(); //线程入口函数

    std::thread thread_;
    EventLoop* loop_;  // 指向该线程创建的 EventLoop 对象
    std::mutex mutex_;
    std::condition_variable cond_;
    bool started_;  // 标记Loop是否已经创建
};
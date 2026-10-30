#include "EventLoopThread.h"
#include "EventLoop.h"
#include <cassert>

EventLoopThread::EventLoopThread()
       :loop_(nullptr),started_(false) {   }

EventLoopThread::~EventLoopThread(){
    // 如果线程正在运行，安全退出
    if( thread_.joinable()){
        // 注意：需要在 EventLoop 中提供 quit（） 方法
        // 此处假设 Loop_ 存在，我们调用 quit（）并等待线程结束
        if(loop_){
           loop_ ->quit();
           thread_.join();
        }
    }
}

EventLoop* EventLoopThread::startLoop() {
    //启动线程
    thread_ = std::thread(&EventLoopThread::threadFunc,this);

    // 等待线程创建好 EventLoop 并通知
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock,[this]{ return started_;  });
    }

    return loop_;
}

void EventLoopThread::threadFunc(){
    // 在子线程中创建 EventLLoop
    EventLoop loop;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        loop_ = &loop;  // 成员指针保存栈对象地址
        started_ = true;
        cond_.notify_one();  // 唤醒主线程 startLoop() 的 cond_.wait()
    }

    // 进入事件循环（阻塞）
    loop.loop();

    //循环结束后，重置指针（但不再使用）
    // Loop_ = nullptr; // 不能这里置空，因为主线程可能还能用，但一般析构时已无法访问
}
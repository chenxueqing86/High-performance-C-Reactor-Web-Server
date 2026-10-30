#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"
#include "EventLoop.h"
#include <cassert>

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop)
         :baseLoop_(baseLoop),
         numThreads_(0),
         started_(false),
         next_(0){}

EventLoopThreadPool::~EventLoopThreadPool(){
    //不需要显示停止，因为 EventLoopThread 的析构会 join 线程
}

void EventLoopThreadPool::setThreadNum(int numThreads){
    assert(!started_ && "Cannot change thread num after started");
    numThreads_ = numThreads;
}

void EventLoopThreadPool::start() {
    assert(!started_);
    assert(baseLoop_ != nullptr);

    started_ = true;

    //如果线程数为 0，则仅使用 baseLoop (单线程模式)
     if(numThreads_ <= 0){
        //不创建额外线程，getNextLoop（）将返回 baseLoop_
        return;
     }

     //创建 numThreads_ 个 EventLoopThread
     threads_.reserve(numThreads_);
     loops_.reserve(numThreads_);

     for(int i = 0;i < numThreads_;++i){
        auto thread = std::make_unique<EventLoopThread>();
        EventLoop* loop = thread->startLoop(); // 启动线程并获取 EventLoop
        loops_.push_back(loop);
        threads_.push_back(std::move(thread));
     }
}

EventLoop* EventLoopThreadPool::getNextLoop() {
    assert(started_);

    //如果没有IO线程，则返回 baseLoop_(主线程处理所有事件)
    if(loops_.empty()){
        return baseLoop_;
    }

    //轮询分发
    EventLoop* loop = loops_[next_];
    next_ = (next_ + 1)% loops_.size();
    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops() const{
    assert(started_);
    if(loops_.empty()){
        return std::vector<EventLoop*>(1,baseLoop_);
    }
    return loops_;
}

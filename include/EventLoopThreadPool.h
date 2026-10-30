#pragma once

#include <vector>
#include <memory>
#include <functional>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool {
    public:
    explicit EventLoopThreadPool(EventLoop* baseLoop);
    ~EventLoopThreadPool();

    // 设置线程数量（必须在 start（） 之前调用）
    void setThreadNum(int numThreads);

    // 启动线程池
    void start();

    // 轮询获取下一个 EventLoop (IO 线程分发)
    EventLoop* getNextLoop();

    //获取所有 EventLoop (用于统计或广播)
    std::vector<EventLoop*> getAllLoops() const;

    //是否已启动
    bool started() const { return started_; }

 private:
      EventLoop* baseLoop_;
      int numThreads_;
      bool started_;
      size_t next_; //轮询索引
      std::vector<std::unique_ptr<EventLoopThread>> threads_;
      std::vector<EventLoop*> loops_;  //所有 IO 线程的 EventLoop 指针
};
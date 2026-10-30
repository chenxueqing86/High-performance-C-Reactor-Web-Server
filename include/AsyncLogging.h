#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>


class AsyncLogging {
    public:
    //获取全局单例
    static AsyncLogging* getInstance(const std::string & basename 
    = "server.log",int flushInterval = 3);
  //  AsyncLogging(const std::string& basename,int flushInterval = 3);
    ~AsyncLogging();

    void start();
    void stop();

    //前端调用：八日至追加到缓冲区
    void append(const char* logline,int len);

    private:
    //获取单例
    AsyncLogging(const std::string& basename,int flushInterval);
    void threadFunc();   // 后台线程入口

    using Buffer = std::vector<char>;
    using BufferPtr = std::unique_ptr<Buffer>;
    using BufferVector = std::vector<BufferPtr>;

    const int flushInterval_;  // 超时刷盘时间 （秒）
    std::atomic<bool> running_;
    std::string basename_;
    std::thread thread_;

    std::mutex mutex_;
    std::condition_variable cond_;

    //双缓冲核心
    BufferPtr currentBuffer_;     // 前端当前写入的缓冲区
    BufferPtr nextBuffer_;   // 备用缓冲区
    BufferVector buffers_;   // 等待后台写入磁盘的缓冲区队列
};


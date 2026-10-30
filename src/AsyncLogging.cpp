#include "AsyncLogging.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/time.h>
#include <unistd.h>

AsyncLogging* AsyncLogging::getInstance(const std::string&
 basename,int flushInterval){
    static AsyncLogging instance(basename,flushInterval);
    return &instance;
 }

AsyncLogging::AsyncLogging(const std::string& basename,int flushInterval)
         : flushInterval_(flushInterval),
            running_(false),
            basename_(basename),
            currentBuffer_(new Buffer),
            nextBuffer_(new Buffer){
                currentBuffer_ ->reserve(1024 * 1024);   //预分配 1MB,避免频繁扩容
                nextBuffer_ ->reserve(1024 * 1024);
            }

AsyncLogging::~AsyncLogging() {
    if(running_){
        stop();
    }
}

void AsyncLogging::start() {
    running_ = true;
    thread_ = std::thread(&AsyncLogging::threadFunc,this);
}

void AsyncLogging::stop() {
    running_ = false;
    cond_.notify_all();
    if(thread_.joinable()){
        thread_.join();
    }
}

void AsyncLogging::append(const char* logline,int len){
    std::lock_guard<std::mutex>  lock(mutex_);
    // 如果当前缓冲区剩余空间够，直接追加
    if(currentBuffer_ -> capacity() - currentBuffer_ ->size() >= (size_t)len){
        currentBuffer_ ->insert(currentBuffer_ ->end(),logline,logline + len);
    }else{
        //空间不够， 把当前缓冲区移入队列，唤醒后台线程
        buffers_.push_back(std::move(currentBuffer_));

        //如果备用缓冲区可用，直接拿来用；否则新建
        if(nextBuffer_){
            currentBuffer_ = std::move(nextBuffer_);
        }else{
            currentBuffer_.reset(new Buffer);
            currentBuffer_->reserve(1024 * 1024);
        }
        currentBuffer_->insert(currentBuffer_ ->end(),logline,logline + len);
        cond_.notify_one(); // 唤醒后台线程落盘
    }
}

void AsyncLogging::threadFunc(){
    FILE* fp = fopen(basename_.c_str(),"ae");  // 'e' 表示 O_CLOEXEC
    if(!fp) {
        perror("fopen log file failed");
        return;
    }

    BufferPtr newBuffer1(new Buffer);
    BufferPtr newBuffer2(new Buffer);
    newBuffer1->reserve(1024 *1024);
    newBuffer2->reserve(1024 * 1024);

    BufferVector buffersToWrite; // 后台专门用来交换的缓冲区
    buffersToWrite.reserve(16);

    while(running_){
      {  std::unique_lock<std::mutex> lock(mutex_);
        if(buffers_.empty()){
            //如果没有数据，等待flushInterval 或者被前端唤醒
            cond_.wait_for(lock,std::chrono::seconds(flushInterval_));
        }

        //把当前缓冲区也塞入队列，确保哪怕没写满也会被定时刷盘
        buffers_.push_back(std::move(currentBuffer_));
        currentBuffer_ = std::move(newBuffer1);

        //交换前台和后台的队列（高效！）
        buffersToWrite.swap(buffers_);

        //补充备用缓冲区
        if(!nextBuffer_){
            nextBuffer_ = std::move(newBuffer2);
        }
    }

    //此时锁已释放，进行磁盘 IO，不会阻塞网络线程！
    for(const auto& buffer : buffersToWrite){
        fwrite(buffer->data(),1,buffer->size(),fp);
    }
    fflush(fp); // 确保落盘

    //清空已经写完的缓冲区
    if(buffersToWrite.size() > 2){
        buffersToWrite.resize(2); //保留两个备用，其他的释放掉
    }
    if(!newBuffer1){
        newBuffer1 = std::move(buffersToWrite.back());
        buffersToWrite.pop_back();
        newBuffer1->clear();
    }
    if(!newBuffer2){
        newBuffer2 = std::move(buffersToWrite.back());
        buffersToWrite.pop_back();
        newBuffer2->clear();
    }
    buffersToWrite.clear();
  }

   //退出前刷盘
   fflush(fp);
   fclose(fp);
}
#include "EventLoopThread.h"
#include "EventLoop.h"
#include <iostream>
#include <chrono>
#include <thread>

int main(){
    EventLoopThread loopThread;
    EventLoop* loop = loopThread.startLoop();

    std::cout << "Main thread: got EventLoop pointer: " << loop << std::endl;

    //从主线程向子线程投递任务
    loop->runInLoop([]() {
        std::cout << "Task executed in EventLoop thread: "
              << std::this_thread::get_id() << std::endl;
    });
    
    //等待任务执行 （简单 sleep，实际可用条件变量）
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    //停止事件循环（需要在 EventLoop 中 添加 quit 方法）
    loop->quit();
    std::cout << "Main thread: done." << std::endl;
    return 0;
}
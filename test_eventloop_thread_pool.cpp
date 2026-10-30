#include "EventLoopThreadPool.h"
#include "EventLoop.h"
#include <iostream>
#include <thread>
#include <chrono>

int main(){
    //主线程的 EventLoop(虽然我们不会运行它，但作为 base 传入)
    EventLoop mainLoop;  //注意：这里会创建 epoll，eventfd 等，但不会运行Loop（）

    EventLoopThreadPool pool(&mainLoop);
    pool.setThreadNum(3);
    pool.start();

    //获取并验证所有EventLoop
    auto allLoops = pool.getAllLoops();
    std::cout << "Total EventLoops: " << allLoops.size() << std::endl;

    //轮询获取 10 次，验证分发是否均匀
    for(int i = 0;i < 10;++i){
        EventLoop* loop = pool.getNextLoop();
        std::cout << "Round" << i << ":EventLoop pointer = " << loop << std::endl;

        //向该 EventLoop 投递一个任务，打印线程ID
       loop->runInLoop([i,loop]() {
            std::cout << "Task" << i << "executed in loop " << loop
                  << " thread ID: " << std::this_thread::get_id() << std::endl;
     });
    }

     // 等待所有任务执行(简单sleep)
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    //退出 （由于 EventLoop 未运行，需手动退出）
    // 实际使用中，EventLoop 会运行在循环中，这里仅用于测试
    std::cout << "Test done." << std::endl;
    return 0;
}
 
    
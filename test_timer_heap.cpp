#include "timer_heap.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <random>

int main(){
    TimerHeap timerHeap;
    std::atomic<int> count(0);
    const int N = 10000;

    //添加 N 个定时器，随机延迟 1~100ms
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1,100);

    auto startAdd = std::chrono::steady_clock::now();
    for(int i = 0;i < N;++i){
        int delayMs = dis(gen);
        timerHeap.addTimer(std::chrono::milliseconds(delayMs),[&count,i]()
    {
        count++;
        // 可以打印进度，但会拖慢测试
    },false);
    }
    auto endAdd = std::chrono::steady_clock::now();
    auto addMs = std::chrono::duration_cast<std::chrono::milliseconds>(endAdd - startAdd).count();
    std::cout << "添加" << N << " 个定时器耗时：" << addMs << "ms\n";

    // 模拟时间推进：每 1ms 调用一次 tick,直到所有定时器触发
    auto startTick = std::chrono::steady_clock::now();
    while(count < N){
        int64_t next = timerHeap.tick();
        if(next > 0){
            // 等待到最近定时器到期（但不超过1ms 精度）
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }else if(next == -1){
            break;//无定时器
        }else{
            //next == 0,说明有定时器到期，继续循环
        }
    }
    auto endTick = std::chrono::steady_clock::now();
    auto tickMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTick - startTick).count();
    std::cout << "触发" << N << " 个定时器耗时:" << tickMs << " ms\n";
    std::cout << "总计耗时： " << (addMs + tickMs) << " ms\n";

    // 验证是否全部触发
    std::cout << "实际触发次数：" << count.load() << std::endl;
    
    return 0;
}


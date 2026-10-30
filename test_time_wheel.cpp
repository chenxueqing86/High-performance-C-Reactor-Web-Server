#include "time_wheel.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <random>

int main(){
    const int N = 100000; //10万个定时器
    const int SLOTS = 1000;
    const int TICK_MS = 10;

    TimeWheel wheel(SLOTS,TICK_MS);
    std::atomic<int> count(0);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1,10000); //延迟 10ms ~ 10000ms

    //======== 添加十万个定时器  ========
    auto startAdd = std::chrono::steady_clock::now();
    for(int i = 0;i < N;i++){
        int delayMs = dis(gen);
        wheel.addTimer(std::chrono::milliseconds(delayMs),[&count,i](){
            count++;
        },false);
    }
    auto endAdd = std::chrono::steady_clock::now();
auto addMs = std::chrono::duration_cast<std::chrono::milliseconds>(endAdd - startAdd).count();
std::cout << "添加" << N << "个定时器耗时： " << addMs << " ms\n";

// ============ 驱动时间轮，直到所有定时器触发 ===========
auto startTick = std::chrono::steady_clock::now();
int ticks = 0;
while(count < N){
    wheel.tick();
    ticks++;
    //每 100 次tick 微眠一次 （让出CPU，模拟真实场景）
    if(ticks % 100 == 0){
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

auto endTick = std::chrono::steady_clock::now();
auto tickMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTick - startTick).count();
       std::cout << "触发" << N << " 个定时器耗时： "  << tickMs << " ms\n";
       std::cout << "总 tick 次数： " << ticks << "\n";
       std::cout << "实际出发次数： " << count.load() << std::endl;
       
       return 0;
}


      
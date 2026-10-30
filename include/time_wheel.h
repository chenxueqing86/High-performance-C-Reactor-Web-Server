#pragma once

#include <vector>
#include <list>
#include <functional>
#include <chrono>
#include <memory>
#include <atomic>
#include <mutex>

class TimeWheel{
    public:
    using TimerCallback = std::function<void()>;
    using TimerId = uint64_t;

    //构造函数：num_slots 槽位数，tick_ms 的毫秒数
    TimeWheel(size_t num_slots = 60,int tick_ms = 1);
    ~TimeWheel() = default;

    //添加定时器，返回ID
    TimerId addTimer(std::chrono::milliseconds delay,TimerCallback cb,bool repeat = false);
    //取消定时器（惰性删除）
    void cancel(TimerId id);
    //驱动时间轮前进一格，返回当前时间（毫秒，用于调试）
    int64_t tick();
    //获取下一次超时的剩余毫秒数（用于epoll_wait 超时）
    int64_t getNextTimeout() const;

    private:
    struct TimerNode {
        TimerId id;
        std::chrono::milliseconds interval;  //重复间隔
        TimerCallback callback;
        bool repeat;
        bool active;
        size_t rotation; //剩余轮次
    };

    using TimerList = std::list<std::shared_ptr<TimerNode>>;

    size_t numSlots_;
    int tickMs_;
    std::vector<TimerList> slots_;
    size_t currentSlot_; //当前指针位置
    TimerId nextId_;
    mutable std::mutex mutex_; // 如果线程安全可加

    //计算定时器应放入的槽位和轮次
    void calcSlotAndRotation(std::chrono::milliseconds delay,size_t& slot,size_t& rotation);
};


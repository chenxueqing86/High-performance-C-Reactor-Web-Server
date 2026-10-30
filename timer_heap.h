#pragma once

#include <vector>
#include <functional>
#include <chrono>
#include <memory>

class TimerHeap {
public:
    using TimerCallback = std::function<void()>;
    using TimerId = uint64_t;

    TimerHeap() = default;
    ~TimerHeap() = default;

    TimerId addTimer(std::chrono::milliseconds delay, TimerCallback cb, bool repeat = false);
    void cancel(TimerId id);
    int64_t tick();
    int64_t getNextTimeout() const;

private:
    struct Node {
        TimerId id;
        std::chrono::steady_clock::time_point expiration;
        std::chrono::milliseconds interval;
        TimerCallback callback;
        bool repeat;
        bool active;
    };

    std::vector<Node> heap_;
    TimerId nextId_ = 1;

    void siftUp(size_t idx);
    void siftDown(size_t idx);
    static bool compare(const Node& a, const Node& b);
};


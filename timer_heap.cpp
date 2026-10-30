#include "timer_heap.h"
#include <algorithm>
#include <chrono>
#include <iostream>

void TimerHeap::siftUp(size_t idx) {
    while (idx > 0) {
        size_t parent = (idx - 1) / 2;
        if (compare(heap_[idx], heap_[parent])) {
            std::swap(heap_[idx], heap_[parent]);
            idx = parent;
        } else {
            break;
        }
    }
}

void TimerHeap::siftDown(size_t idx) {
    size_t size = heap_.size();
    while (true) {
        size_t left = 2 * idx + 1;
        size_t right = 2 * idx + 2;
        size_t smallest = idx;
        if (left < size && compare(heap_[left], heap_[smallest]))
            smallest = left;
        if (right < size && compare(heap_[right], heap_[smallest]))
            smallest = right;
        if (smallest != idx) {
            std::swap(heap_[idx], heap_[smallest]);
            idx = smallest;
        } else {
            break;
        }
    }
}

bool TimerHeap::compare(const Node& a, const Node& b) {
    return a.expiration < b.expiration;
}

TimerHeap::TimerId TimerHeap::addTimer(std::chrono::milliseconds delay,
                                      TimerCallback cb,
                                      bool repeat) {
    auto now = std::chrono::steady_clock::now();
    Node node;
    node.id = nextId_++;
    node.expiration = now + delay;
    node.interval = delay;
    node.callback = std::move(cb);
    node.repeat = repeat;
    node.active = true;
    heap_.push_back(std::move(node));
    siftUp(heap_.size() - 1);
    return node.id;
}

void TimerHeap::cancel(TimerId id) {
    for (auto& node : heap_) {
        if (node.id == id && node.active) {
            node.active = false;
            break;
        }
    }
}

int64_t TimerHeap::tick() {
    auto now = std::chrono::steady_clock::now();
    while (!heap_.empty()) {
        Node& top = heap_[0];
        if (!top.active) {
            std::pop_heap(heap_.begin(), heap_.end(), compare);
            heap_.pop_back();
            continue;
        }
        if (top.expiration > now) {
            break;
        }
        std::pop_heap(heap_.begin(), heap_.end(), compare);
        Node expired = std::move(heap_.back());
        heap_.pop_back();

        if (expired.callback) {
            try {
                expired.callback();
            } catch (...) {}
        }
        if (expired.repeat && expired.active) {
            expired.expiration = std::chrono::steady_clock::now() + expired.interval;
            expired.active = true;
            heap_.push_back(std::move(expired));
            siftUp(heap_.size() - 1);
        }
    }
    return getNextTimeout();
}

int64_t TimerHeap::getNextTimeout() const {
    if (heap_.empty()) return -1;
    auto now = std::chrono::steady_clock::now();
    auto diff = heap_[0].expiration - now;
    if (diff <= std::chrono::milliseconds(0)) return 0;
    return std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
}


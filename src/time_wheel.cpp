#include "time_wheel.h"
#include <algorithm>
#include <iostream>
#include <cmath>

TimeWheel::TimeWheel(size_t num_slots,int tick_ms)
      :numSlots_(num_slots),tickMs_(tick_ms),
      slots_(num_slots),currentSlot_(0),nextId_(1){}

TimeWheel::TimerId TimeWheel::addTimer(std::chrono::milliseconds delay,
                                     TimerCallback cb,
                                    bool repeat){
        auto node = std::make_shared<TimerNode>();
        node->id = nextId_++;
        node->interval = delay;
        node->callback = std::move(cb);
        node->repeat = repeat;
        node->active = true;

        size_t slot;
        size_t rotation;
        calcSlotAndRotation(delay,slot,rotation);
        node->rotation = rotation;

        //如果定时器在同一个槽位切轮次为0，则应该立即触发（但至少也要等到下一圈）
        //这里我们将其放入当前槽位，轮次为0，下次tick 触发
        if(rotation == 0 && slot == currentSlot_){
            //延迟为 0 或小于一个tick，放入当前槽位，但会在下一次 tick触发
            // 为防止立即触发，可以放入下一个槽位？但这里我们保持原样，由tick处理
        }

        std::lock_guard<std::mutex> lock(mutex_);
        slots_[slot].push_back(node);
        return node->id;
}


void TimeWheel::calcSlotAndRotation(std::chrono::milliseconds delay,size_t& slot,size_t& rotation)
{
    int totalTicks = delay.count() /tickMs_;
    if(totalTicks == 0) totalTicks =  1; //至少延迟一个tick

    //从当前槽位开始计算目标槽位
    slot = (currentSlot_ + totalTicks) % numSlots_;
    rotation = totalTicks / numSlots_;
    //如果正好落在当前槽位且延迟 》0，则轮次可能为 0 ，但需要至少一圈？
    // 例如 numSlots = 60,currentSlot = 0,delay = 60ms,则 totalTicks = 60，slot = 0，rotation= 1
    // 若 delay = 59ms，则totalTicks = 59，slot= 59，rotation = 0
    // 若 delay = 1ms，则totalTicks = 1，slot = 1，rotation = 0
}

void TimeWheel::cancel(TimerId id){
    std::lock_guard<std::mutex> lock(mutex_);
    for(auto& slot :slots_){
        for(auto& node:slot){
            if(node->id == id && node->active){
                node->active = false;
                return;
            }
        }
    }
}


int64_t TimeWheel::tick() {
    std::lock_guard<std::mutex> lock(mutex_);
    //移动指针
    currentSlot_ = (currentSlot_ + 1)% numSlots_;
    auto& slot = slots_[currentSlot_];
    //遍历槽位链表
    auto it = slot.begin();
    while(it != slot.end()){
        auto& node = *it;
        if(!node->active){
            it = slot.erase(it);
            continue;
        }
        if(node->rotation > 0){
            node->rotation--;
            ++it;
            continue;
        }
        //rotation == 0,触发定时器
        // if(node->callback){
        //     try{
        //         node->callback();
        //     }catch(...){
        //          // 防止回调抛异常导致定时器链断裂
        //      }
        // }
        if (node->callback)
{
    try
    {
        node->callback();
    }
    catch (...)
    {
        // 防止回调抛异常导致定时器链断裂
    }
}

        if(node->repeat){
            //重复定时器： 重新计算位置
            //注意：重新插入到未来莫槽位，rotation 基于当前currentSlot_ 计算
            size_t newSlot,newRotation;
            calcSlotAndRotation(node->interval,newSlot,newRotation);

            //但calc 使用了 currentSlot_,它已经前进了一位
            //如果 newSlot == currentSlot_ 且 newRotation == 0,说明定在了当前槽位
            // 会立即再次触发，需要额外加一圈或加一个tick
            if(newSlot == currentSlot_ && newRotation == 0){
                newRotation = 1;
                newSlot = (currentSlot_ + 1) % numSlots_;
            }

            node->rotation = newRotation;
            node->active = true;
            slots_[newSlot].push_back(node);
            it = slot.erase(it);
        }else{
            //单次定时器，直接删除
            it = slot.erase(it);
        }
    }
    return currentSlot_;
}

int64_t TimeWheel::getNextTimeout() const{
    std::lock_guard<std::mutex> lock(mutex_);
    //检查是否有任何活跃定时器
    for(const auto& slot : slots_){
        for(const auto& node:slot){
            if(node->active){
                //有定时器存在，最多等待一个tick周期
                return tickMs_;
            }
        }
    }
    return -1; //无定时器
}
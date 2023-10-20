#include "ThreadLoadCounter.h"

#include "util/Util.h"

namespace avc {
namespace util {

ThreadLoadCounter::ThreadLoadCounter(int maxCount, int maxUsec) {
    last_wakeup_time_ = last_sleep_time_ = getCurrentMicrosecond();
    max_size_ = maxCount;
    max_usec_ = maxUsec;
}

ThreadLoadCounter::~ThreadLoadCounter() {}

/**
 * 在线程执行例程中，进入休眠之前调用
*/
void ThreadLoadCounter::onSleep() {
    #if 0
    auto now = getCurrentMicrosecond();
    /**
     * 此时属于唤醒结束，记录唤醒结束时间
     *      sleeping == false，判断是唤醒结束
    */
    if (!sleeping_) {
        sleeping_ = true;
        //没有处于休眠，所以记录唤醒结束时间
        time_records_.push_back(TimeRecord(TimeRecord::kWakeupEnd, now));
    }
    /**
     * 此时准备进入休眠，记录休眠开始时间
     * */       
    time_records_.push_back(TimeRecord(TimeRecord::kSleepBegin, now));
    #else 
        auto current = getCurrentMicrosecond();

        LOCK_GUARD(mutex_);
        if (!sleeping_) {
            //唤醒结束，计算本次执行时间
            time_records_.push_back(TimeRecord(sleeping_, current - last_wakeup_time_));
            if (time_records_.size() > max_size_) {
                time_records_.pop_front();
            }
        }
        sleeping_ = true;
        last_sleep_time_ = current;
    #endif
}
/**
 * 在线程执行例程中，唤醒后调用
*/
void ThreadLoadCounter::onWakeup() {
    #if 0
    auto now = getCurrentMicrosecond();
    /**
     * 此时属于休眠结束，记录休眠结束时间
    */
   if (sleeping_) {
        sleeping_ = false;
        /**
         * 此时属于休眠结束，记录休眠结束时间
        */
        time_records_.push_back(TimeRecord(TimeRecord::kSleepEnd, now));
   }
 
    /**
     * 此时属于唤醒开始，记录唤醒开始时间
    */
    time_records_.push_back(TimeRecord(TimeRecord::kWakeupBegin, now));
    #else
    auto current = getCurrentMicrosecond();
    LOCK_GUARD(mutex_);
    if (sleeping_) {
        //睡眠结束, 记录上次本次休眠时间
        time_records_.push_back(TimeRecord(sleeping_, current - last_sleep_time_));
        if (time_records_.size() > max_size_) {
            time_records_.pop_front();
        }
    }
    sleeping_ = false;
    last_wakeup_time_ = current;
    #endif
}
 
/**
 * 返回线程的负载，即CPU使用率
*/
int ThreadLoadCounter::load() {
    uint64_t totalRuntime = 0;
    uint64_t totalSleepTime = 0;

    auto current = getCurrentMicrosecond();
    LOCK_GUARD(mutex_);
    /**
     * 统计样本中的所有运行时间与休眠时间
     *      统计样本时间时，使用使用最近的事件计算
    */

    /**
     * load函数调用时，当前时刻产生的时间
     *      sleeping_，用于判断当前执行线程处于唤醒还是休眠状态
    */
    if (sleeping_) {
        totalSleepTime += current - last_wakeup_time_;
    } 
    else {
        totalRuntime += current - last_sleep_time_;
    }

    /**
     * 统计所有已存在的样本时间
    */
    for (auto it = time_records_.rbegin(); it != time_records_.rend(); ++it) {
        if (it->sleep_) {
            totalSleepTime += it->time_;
        } 
        else {
            totalRuntime += it->time_;
        }
    }

    /**
     * 统计样本时间，超过统计窗口
     *      则移除最早的统计时间
    */
    auto totalTime = totalRuntime + totalSleepTime;
    while ((totalTime > max_usec_) && (time_records_.size() > 0)) {
        const auto &node = time_records_.front();
        totalTime -= node.time_;
        if (node.sleep_) {
            totalSleepTime -= node.time_;
        }
        else {
            totalRuntime -= node.time_;
        }
        time_records_.pop_front();
    }

    if (totalTime == 0) {
        return 0;
    } 
    return (int)(((float)totalRuntime / totalTime) * 100);
}

}
}
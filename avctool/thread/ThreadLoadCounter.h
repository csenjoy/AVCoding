#ifndef THREAD_THREADLOADCOUNTER_H
#define THREAD_THREADLOADCOUNTER_H

#include <list>
#include "util/MutexWrapper.h"

namespace avc {
namespace util {


/**
 * 线程负载计算
 * 
 *  该方案通过记录cpu休眠与唤醒时刻，用于后续计算cpu负载：
 *      线程CPU使用率计算（即线程负载）
 *      休眠时间：记录休眠前后的时间，即可得到休眠时间： 休眠开始记录的时间 - 休眠结束记录的时间
 *      执行时间：唤醒结束记录的时间 - 唤醒开始记录的时间
 *      CPU使用率： （执行时间 / (休眠时间 + 执行时间)） * 100%
 * 
 *  上述方案不利于计算负载，修正后的方案：
 *      统计cpu执行情况：休眠时间与运行时间的样本数量
 *      CPU使用率: 计算一段时间内，运行时间所占比例
 *      
*/
class ThreadLoadCounter {
public:
    /**
     * @param maxCount 限制记录时间节点的数量(即list的最大数量)
     * @param maxUsec  统计cpu负载时的窗口大小，例如2s这个时间内，cpu使用情况
    */
    explicit ThreadLoadCounter(int maxCount = 32, int maxUsec = 2 * 1000 * 1000);
    ~ThreadLoadCounter();

    /**
     * 在线程执行例程中，进入休眠之前调用
    */
    void onSleep();
    /**
     * 在线程执行例程中，唤醒后调用
    */
    void onWakeup();
    /**
     * 返回线程的负载，即CPU使用率
    */
    int load();
private:
    /**
     * onSleep与onWakeup有执行线程调用，用于记录线程执行时间情况
     *      load()调用时，会出现竞争条件，所以需要加锁
     *      在没有load调用情况下，没有竞争条件
    */
    MutexWrapper<std::mutex> mutex_;
    bool sleeping_ =  true;
    uint64_t last_sleep_time_;
    uint64_t last_wakeup_time_;

    struct TimeRecord {
        #if 0
        enum {
            kWakeupBegin,
            kWakeupEnd,
            kSleepBegin,
            kSleepEnd
        };

        TimeRecord(int state, uint64_t time) : state_(state), time_(time) {

        }
        

        int state_;
        #else
        TimeRecord(bool sleep, uint64_t time) : sleep_(sleep), time_(time) {}
        bool sleep_;//用于判断是休眠时间还是执行时间
        #endif
        uint64_t time_;
    };//struct TimeRecord
    /**
     * 根据线程状态，以此记录各个时间节点
     *      （1）list长度应该如何限制? 
     *              list记录执行时间与休眠时间，因此使用max_size_控制最大数量即可
    */
    std::list<TimeRecord> time_records_;
    int max_size_;//避免list数量过大
    int max_usec_;//计算cpu负载的时间窗口
};//class ThreadLoadCounter


}
}

#endif
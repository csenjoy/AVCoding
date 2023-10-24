#ifndef UTIL_TIMER_H
#define UTIL_TIMER_H

#include <memory>

#include "poller/EventPollerPool.h"

/**
 * 定时器实现： 依赖EventPoller提供的DelayTask实现
*/
namespace avc {
namespace util {

class Timer : public std::enable_shared_from_this<Timer>{
public:
    using Ptr = std::shared_ptr<Timer>;
    using OnTimer = std::function<void()>;
    
    AVC_STATIC_CREATOR(Timer)
    ~Timer();

    void start(int milliseconds);
    void stop();
private:
    Timer(OnTimer &&onTimer, const EventPoller::Ptr &poller);

    void onTime();
private:
    /**
     * Timer通过EventPoller::DelayTask实现
    */
    EventPoller::DelayTask::Ptr delay_task_;
    OnTimer on_timer_;

    EventPoller::Ptr poller_;
};//class Timer

}
}



#endif
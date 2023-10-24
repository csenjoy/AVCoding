#include "Timer.h"

#include "log/Log.h"

namespace avc {
namespace util {

Timer::~Timer() {
    stop();
}

void Timer::start(int milliseconds) {
    if (delay_task_ == nullptr) {

        std::weak_ptr<Timer> self = shared_from_this();
        delay_task_ = poller_->addDelayTask(milliseconds, [self, milliseconds, this]()->uint64_t {
                auto timer = self.lock();
                if (timer) {
                    timer->onTime();
                    return milliseconds;
                }
                return 0;
            });
    }
}

void Timer::stop() {
    if (delay_task_) {
        delay_task_->cancel();
        delay_task_ = nullptr;
    }
}

Timer::Timer(OnTimer&& onTimer, const EventPoller::Ptr& poller) 
    :on_timer_(std::move(onTimer)), poller_(poller) {
}

void Timer::onTime() {
    try {
        if (on_timer_) {
            on_timer_();
        }
    }
    catch (...) {
        WarnL << "on_timer callback failed";
    }
}

}//namespace util
}
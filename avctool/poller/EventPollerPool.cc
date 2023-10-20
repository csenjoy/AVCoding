#include "EventPollerPool.h"

#include "thread/ThreadPool.h"

namespace avc {
namespace util {

EventPollerPool::~EventPollerPool() {
}

EventPollerPool &EventPollerPool::instance() {
    // TODO: insert return statement here
    static EventPollerPool::Ptr s_instance = EventPollerPool::create();
    return *s_instance;
}

EventPoller::Ptr EventPollerPool::getEventPoller() {
    return std::static_pointer_cast<EventPoller>(getTaskExecutor());
}


EventPollerPool::EventPollerPool() {
    int hardware_concurrency = std::thread::hardware_concurrency();
    for (int index = 0; index < hardware_concurrency; ++index) {
        addEventPoller(StrPrinter << "EventPollerPool#" << index, ThreadPool::PRIORITY_HIGHEST, true);
    }
}

int EventPollerPool::addEventPoller(const std::string &name, int priority, bool cpuAffinity) {
    auto eventPoller = EventPoller::create();
    if (eventPoller) {
        eventPoller->runLoop();
        eventPoller->async_first([name, priority, cpuAffinity]()->void {
            setThreadName(name.c_str());
            ThreadPool::setThreadPriority(priority);

            TraceL << "EventPoller started.";
        });

        task_executors_.push_back(eventPoller);
        return 0;
    }
    WarnL << "Failed to call EventPoller::create()";
    return -1;
}


} // namespace util
}
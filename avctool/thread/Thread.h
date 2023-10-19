#ifndef THREAD_THREAD_H
#define THREAD_THREAD_H

#include <thread>
#include <memory>
#include <functional>

#include "util/Util.h"

namespace avc {
namespace util {

class MessageQueue;    

class Thread {
public:
    enum Priority {
        PRIORITY_LOWEST = 0,
        PRIORITY_LOW,
        PRIORITY_NORMAL,
        PRIORITY_HIGH,
        PRIORITY_HIGHEST
    };//enum Priority

    using Ptr = std::shared_ptr<Thread>;
    using OnCallback = std::function<void()>;

    template<class ...ARGS>
    static Ptr create(ARGS &&...args) {
        return std::make_shared<Thread>(std::forward<ARGS>(args)...);
    }

    explicit Thread(const std::string& threadName, int priority = Priority::PRIORITY_NORMAL) {
        onStarted_ = [threadName, priority, this]()->void {
            setThreadName(threadName.c_str());
            onStarted();
        };

        onFinished_ = [this]()->void {
            onFinished();
            //exit();
        };

        thread_ = std::make_shared<std::thread>(
                [this]()->void {
                    onStarted_();
                    run();
                    onFinished_();
                }
            );
    }

    virtual ~Thread() {
        exit();
    }

    void exit() {
        if (thread_ && thread_->joinable()) {
            thread_->join();
            thread_.reset();
        }
    }
protected:
    virtual void run() = 0;
    virtual void onStarted() {}
    virtual void onFinished() {}
private:
    OnCallback onStarted_;
    OnCallback onFinished_;

    std::shared_ptr<std::thread> thread_;
};//class Thread

}
}

#endif
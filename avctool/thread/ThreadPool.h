#ifndef THREAD_THREADPOOL_H
#define THREAD_THREADPOOL_H

#include <memory>
#include <vector>
#include <thread>
#include <functional>
#include <chrono>
#include <list>

#include "util/Util.h"
#include "thread/TaskExecutor.h"

namespace avc {
namespace util {

class ThreadPool : public TaskExecutor, 
                   public std::enable_shared_from_this<ThreadPool>, 
                   Nocopyable {
public:
    enum Priority {
        PRIORITY_LOWEST = 0,
        PRIORITY_LOW,
        PRIORITY_NORMAL,
        PRIORITY_HIGH,
        PRIORITY_HIGHEST
    };//enum Priority

    static bool setThreadPriority(int priority) { return false; }

    using Ptr = std::shared_ptr<ThreadPool>;
    using OnCallback = std::function<void(int)>;

    template<class ...ARGS>
    static Ptr create(ARGS &&...args) {
        return std::make_shared<ThreadPool>(std::forward<ARGS>(args)...);
    }

    explicit ThreadPool(unsigned capacity, int priority = Priority::PRIORITY_NORMAL, OnCallback &&onStart = nullptr) :mutex_(true) {
        setOnStart(std::move(onStart));

        auto hardware_concurrency = std::thread::hardware_concurrency();
        if (capacity > hardware_concurrency) {
            capacity = hardware_concurrency;
        }

        for (auto index = 0; index < capacity; ++index) {
            threads_.emplace_back([=]() {
                run(index);
                });
        }
    }

    virtual ~ThreadPool() {
        shutdown();
    }

    void setOnStart(OnCallback&& onStart) {
        if (onStart) {
            onStart_ = std::move(onStart);
        }
        else {
            onStart_ = [this](int index) {
                setThreadName((StrPrinter << "ThreadPool#" << index).c_str());
            };
        }
    }

    Task::Ptr async(TaskIn&& task, bool may_sync = true) override {
        if (may_sync && currentThread()) {
          if (task) task();
          return nullptr;
        }

        Task::Ptr job = Task::create(std::move(task));
        {
            LOCK_GUARD(mutex_);
            pedding_.push_back(job);
        }
        sem_.post();
        return job;
    }
    
    Task::Ptr async_first(TaskIn&& task, bool may_sync = true) override {
        if (may_sync && currentThread()) {
            if (task) task();
            return nullptr;
        }

        Task::Ptr job = Task::create(std::move(task));
        {
            LOCK_GUARD(mutex_);
            pedding_.push_front(job);
        }
        sem_.post();
        return job;
    }
private:
    void shutdown() {
        exit_ = true;
        sem_.post(threads_.size());

        for (auto& thread : threads_) {
            thread.join();
        }
    }

    void run(int index) {
        if (onStart_) onStart_(index);

        while (!exit_) {
            /**
             * 线程池无需，记录线程负载
             *      (1) 线程负载时某个线程的属性，而不是线程池的属性
             *     （2）信号量post时，应该会负载均衡的唤醒等待线程？？？（避免某个线程一直BUSY)
             *     （3）线程池不需要负载均衡：
             *            单个任务来临时，一直是某个线程处理，不存在负载均衡
             *            多个任务来临时，idle的线程会获取某个任务执行，也不存在负载均衡
             *            线程池的模型：多个线程等待获取任务处理，不存在负载均衡的问题
            */
            //onSleep();
            sem_.wait();
            //onWakeup();

            Task::Ptr job;
            {
                LOCK_GUARD(mutex_);
                if (!pedding_.empty()) {
                    job = pedding_.front();
                    pedding_.pop_front();
                }
            }
#if 0
            //为了测试任务取消，这里延迟执行任务，以便取消任务（cancel接口先执行）  
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
#endif
            if (job && (*job)) {
                (*job)();
            }
        }
    }

    bool currentThread() const {
        bool current = false;
        for (auto& thread : threads_) {
            if (std::this_thread::get_id() == thread.get_id()) {
                current = true;
                break;
            }
        }
        return current;
    }
private:
    bool exit_ = false;

    Semphore sem_;
    //std::mutex mutex_;
    MutexWrapper<std::mutex> mutex_;
    std::list<Task::Ptr> pedding_;

    std::vector<std::thread> threads_;
    OnCallback onStart_;    
};//class ThreadPool

}
}


#endif
#ifndef THREAD_TASKEXECUTOR_H
#define THREAD_TASKEXECUTOR_H

#include <functional>

#include "thread/TaskCancelable.h"
#include "util/Semphore.h"
#include "util/OnceToken.h"

namespace avc {
namespace util {

/**
 * 任务执行器： 提供任务执行接口：包括同步与异步
*/
class TaskExecutorInterface {
public:
    /**
     * 定义任务类型: TaskIn与Task的签名需要保持一致
    */
    using TaskIn = std::function<void()>;
    using Task = TaskCancelableImpl<void()>;
 
    virtual ~TaskExecutorInterface() {}

    /**
     * 异步执行任务
     * @param may_sync 如果调用async的线程与任务执行器处于同一线程，则直接执行;
     *                 否则投递到任务队列
    */
    virtual Task::Ptr async(TaskIn &&task, bool may_sync = true) = 0;
    /**
     * 最高优先级异步执行任务 
    */
    virtual Task::Ptr async_first(TaskIn &&task, bool may_sync = true) = 0;
    /**
     * 同步执行任务
     * @param task 使用const TaskIn &类型是因为是同步任务
     * @note 同步执行任务与may_sync有所区别： may_sync表示如果处于相同线程则立马执行
     *                                      而sync等待任务执行完成（在Executor线程中执行）
    */
    virtual void sync(const TaskIn& task) {
        Semphore sem;
        async([&]()->void {
            //task()函数调用可能抛出异常，因此使用RAII机制唤醒等待的调用线程
            OnceToken onceToken(nullptr, [&]()->void {
                sem.post();
                });

            task();
            //sem.post();
        });
        sem.wait();
    }
    /**
     * 最高优先级同步执行任务
    */
    virtual void sync_first(const TaskIn& task) {
        Semphore sem;
        async_first([&]()->void {
            //task()函数调用可能抛出异常，因此使用RAII机制唤醒等待的调用线程
            OnceToken onceToken(nullptr, [&]()->void {
                sem.post();
            });
            task();
        });
        sem.wait();
    }
};//class TaskExecutorInterface

class TaskExecutor : public TaskExecutorInterface {
public:
    using Ptr = std::shared_ptr<TaskExecutor>;

    virtual ~TaskExecutor() {}
};//class TaskExecutor

}//namespace util
}

#endif
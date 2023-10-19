#ifndef POLLER_EVENTPOLLER_H
#define POLLER_EVENTPOLLER_H

#include <thread>
#include <memory>
#include <list>
#include <unordered_map>

#include "thread/TaskExecutor.h"
#include "poller/PipeWrapper.h"//使用PipeWrapper，支持写Pipe唤醒轮询函数（select或epoll_wait)
#include "util/MutexWrapper.h"
#include "util/Nocopyable.h"

#include "log/Log.h"

#define HAS_EPOLL  0


#if HAS_EPOLL

#define EPOLL_SIZE 1024
#define TO_EPOLL_EVENT(events)  ((events) & avc::util::EventPoller::Event::kEventRead) ? EPOLLIN : 0 \
                               |((events) & avc::util::EventPoller::Event::kEventWrite) ? EPOLLOUT : 0 \
                               |((events) & avc::util::EventPoller::Event::kEventError) ? EPOLLERR : 0

#define TO_POLLER_EVENT(events) ((events) & EPOLLIN) ? avc::util::EventPoller::Event::kEventRead : 0 \
                               |((events) & EPOLLOUT) ? avc::util::EventPoller::Event::kEventWrite : 0 \
                               |((events) & EPOLLERR) ? avc::util::EventPoller::Event::kEventError : 0 
#endif


namespace avc {
namespace util {

/**
 * 事件轮询: 通过调用select或epoll_wait接口，轮询网络I/O时间或者定时器事件
*/
class EventPoller : public TaskExecutor,
                    public std::enable_shared_from_this<EventPoller>,
                    Nocopyable {
public:
    enum Event {
        kEventRead = 0x1,
        kEventWrite = 0x2,
        kEventError = 0x4,
    };//enum Event

    using Ptr = std::shared_ptr<EventPoller>;
    /**
     * int参数用于通知发生的事件，由于用户使用函数对象注册回调通知，
     * 用户自己会注册自己的数据（类似C中的void *param）。因此仅需要一个参数（就绪的事件）即可
    */
    using OnEvent = std::function<void(int)>;
    using FD = int;

    AVC_STATIC_CREATOR(EventPoller)

    virtual ~EventPoller();

    /**
     * 提供轮询接口: 等待网络I/O事件
     * @param blocked 如果为true，则阻塞调用线程，直到收到退出事件
     *                如果为false，则创建线程后，调用runLoop(true)
    */
    void runLoop(bool blocked = false);

    /**
     * 退出循环
    */
    void shutdown();

    /**
     * 注册网络I/O事件
    */
    int attachEvent(int fd, int events, OnEvent &&cb);
    int detachEvent(int fd);
    int modifyEvent(int fd, int events);

    /**
     * 异步事件投递到任务队列后，通过管道的方式唤醒runLoop
     *      因为runLoop是阻塞在select或epoll_wait上，因此需要fd事件
     *          1）对于Window平台，轮询函数需要网络套接字才能唤醒，因此管道需要使用套接字模拟实现
     *          2）对于Linux平台，轮询函数使用文件描述符唤醒，因此使用管道即可
    */
    Task::Ptr async(TaskIn&& task, bool may_sync = true) override;
    Task::Ptr async_first(TaskIn&& task, bool may_sync) override;
private:
    EventPoller();
    /**
     * EventPoller::runLoop执行任务时，捕获退出异常，从而完成退出操作
    */
    class Exit : public std::exception {

    };//class Exit

    /**
     * writePipe仅仅用于唤醒轮询函数，写入的数据目前没有定义格式
    */
    void writePipe();

    bool currentThread() const;

    /**
     * 调度延迟任务
     * @return 返回最近定时器超时时间, 单位毫秒
     *         -1代表没有定时任务
    */
    int64_t scheduleDelayTask();

    /**
     * 管道也属于文件I/O事件
    */
    void onPipeEvent(); 
    void attachPipeEvent();

    /**
     * 注册文件I/O事件，以及回调函数
    */
    struct EventRecord {
        using Ptr = std::shared_ptr<EventRecord>;
        AVC_STATIC_CREATOR(EventRecord)
        
        EventRecord(int events, OnEvent &&cb): events_(events), cb_(std::move(cb)) {
        }

        bool readable() const {
            return events_ & Event::kEventRead;
        }

        bool writeable() const {
            return events_ & Event::kEventWrite;
        }

        bool exceptable() const {
            return events_ & Event::kEventError;
        }

        //FD fd_;
        /**
         * 文件描述符关心的事件类型
        */
        int events_;
        /**
         * 文件描述符就绪的事件
         * @note 通过此字段收集文件描述符对应的就绪事件。
        */
        int signaled_events_ = 0;
        OnEvent cb_;
    };//struct EventCallbackRecord

    int attachEvent_l(int fd, EventRecord::Ptr eventRecord);
private:
    Semphore sem_started_;
    std::shared_ptr<std::thread> thread_;

    /**
    * 任务队列
    */
    std::list<Task::Ptr> tasks_;
    MutexWrapper<std::mutex> tasks_mutex_;

    std::unordered_map<FD, EventRecord::Ptr> event_records_;
    MutexWrapper<std::mutex> event_records_mutex_;
    PipeWrapper pipe_;

    bool exit_ = false;

#if HAS_EPOLL
    int epoll_fd_ = -1;
#endif
};//class EventPoller

}//namespace util
}//namespace avc

#endif
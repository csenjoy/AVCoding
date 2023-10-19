#include "EventPoller.h"

#include <assert.h>

#include "log/Log.h"

#include "network/SockUtil.h"
#include "poller/SelectWrapper.h"
#if HAS_EPOLL
#include "poller/EpollWrapper.h"
#endif

#include "error/uv_errno.h"

namespace avc {
namespace util {

EventPoller::~EventPoller() {
    shutdown();
}

void EventPoller::runLoop(bool blocked) {
    if (blocked) {
        while (!exit_) {
            /**
             * next下次轮询函数唤醒时间，单位毫秒
             * next==0时，表示永久阻塞(即没有超时唤醒)
            */
            auto next = scheduleDelayTask();

            //文件I/O时间唤醒
            std::list<EventRecord::Ptr> signaledEventRecords;
#if HAS_EPOLL
            struct epoll_event signaled_events[EPOLL_SIZE];
            int n = epoll_wait(epoll_fd_, signaled_events, EPOLL_SIZE, next > 0 ? next : -1); 
            if (n > 0) {
                for (int index = 0; index < n; ++index) {
                    EventRecord::Ptr signaledEventRecord;
                    {
                        //fake lock
                        LOCK_GUARD(event_records_mutex_);
                        auto node = event_records_.find(signaled_events[index].data.fd);
                        if (node == event_records_.end()) {
                            /**
                             * 在注册的事件记录中没有找到对应文件描述符，可能是被移除了
                             * 移除该文件描述符的事件记录（主要是出发epoll_ctl删除注册的事件)
                            */
                            detachEvent(signaled_events[index].data.fd);
                            continue;
                        }
                        signaledEventRecord = node->second;
                    }
                    assert(signaledEventRecord);
                    signaledEventRecord->signaled_events_ = TO_POLLER_EVENT(signaled_events[index].events);
                    signaledEventRecords.push_back(signaledEventRecord);
                }
            }
            else {
                //超时或者出错
                continue;
            }
            
#else 
            //准备好需要监听的套接字集合
            FdSet readSet, writeSet, exceptSet; FD maxFd = 0;
            {
                LOCK_GUARD(event_records_mutex_);
                for (auto& record : event_records_) {
                    if (record.first > maxFd) maxFd = record.first;
                
                    if (record.second->readable()) {
                        readSet.addFd(record.first);
                    }
                    if (record.second->writeable()) {
                        writeSet.addFd(record.first);
                    }
                    if (record.second->exceptable()) {
                        exceptSet.addFd(record.first);
                    }
                }
            }
                
            struct timeval tv;
            tv.tv_sec = next / 1000L;
            tv.tv_usec = (next % 1000L) * 1000;
            int ret = Select(maxFd + 1, &readSet, &writeSet, &exceptSet, next > 0 ? &tv : nullptr);
            if (ret <= 0) {
                //超时唤醒，或者出错-1
                /**
                 * @Todo 此处需要检查出错原因吗?
                */
                continue;
            }

            {
                //fake lock 
                LOCK_GUARD(event_records_mutex_);
                for (auto &record : event_records_) {
                    record.second->signaled_events_ = 0;                    
                    if (readSet.hasFd(record.first)) {
                        record.second->signaled_events_ |= Event::kEventRead;
                    }
                    if (writeSet.hasFd(record.first)) {
                        record.second->signaled_events_ |= Event::kEventWrite;
                    }
                    if (exceptSet.hasFd(record.first)) {
                        record.second->signaled_events_ |= Event::kEventError;
                    }

                    if (record.second->signaled_events_ > 0) {
                        signaledEventRecords.push_back(record.second);
                    }
                }
            }
#endif
            for (auto& signaled : signaledEventRecords) {
                try {
                    signaled->cb_(signaled->signaled_events_);
                }
                catch (...) {
                    WarnL << "Handle signaled event callback error.";
                }
            }
        }
    }
    else {
        exit_ = false;
        thread_ = std::make_shared<std::thread>([this]()->void {
            setThreadName((StrPrinter << "EventPoller#" << this).c_str());

            sem_started_.post();
            runLoop(true);
        });
        sem_started_.wait();
    }
}

/**
 * 退出循环
*/
void EventPoller::shutdown() {
    async([this]()->void {
        throw Exit();
    });

    if (thread_ && thread_->joinable()) {
        thread_->join();
        thread_ = nullptr;
    }

#if HAS_EPOLL
    epoll_close(epoll_fd_);
    epoll_fd_ = -1;
#endif
}

/**
 * 注册网络I/O事件
*/
int EventPoller::attachEvent(int fd, int events, OnEvent &&cb) {
    if (fd < 0) {
        TraceL << "Invalid fd";
        return -1;
    }

    auto eventRecord = EventRecord::create(events, std::move(cb));
    if (eventRecord == nullptr) {
        TraceL << "Out of memory";
        return -1;
    }

    return attachEvent_l(fd, eventRecord);
}

int EventPoller::detachEvent(int fd) {
    if (currentThread()) {
        int ret = 0;
        //fake lock
        LOCK_GUARD(event_records_mutex_);
#if HAS_EPOLL
        ret = epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
        if (ret == -1) {
            WarnL << "Failed to epoll_ctl del fd" << get_uv_errmsg();
        }
#endif
        event_records_.erase(fd);
        return ret;
    }

    async([fd, this]()->void {
        detachEvent(fd);
    });
    return 0;
}

int EventPoller::modifyEvent(int fd, int events) {
    if (currentThread()) {
        int ret = 0;
        //fake lock
        LOCK_GUARD(event_records_mutex_);
#if HAS_EPOLL
        struct epoll_event event = { 0 };
        event.events = TO_EPOLL_EVENT(events);
        event.data.fd = fd;

        /**
         * 如果fd对应的event_record不存在，轮询函数触发此fd事件后，移除该fd
        */
        ret = epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &event);
        if (ret == -1) {
            WarnL << "Failed to epoll_ctl modify fd" << get_uv_errmsg();
        }
#endif
        /**
         * 查找到fd对应的event_record，修改event_records当中的events字段
        */
        auto node = event_records_.find(fd);
        if (node != event_records_.end()) {
            node->second->events_ = events;
        }
        return ret;
    }

    async([fd, events, this]()->void {
        modifyEvent(fd, events);
    });
    return 0;
}

/**
 * 异步事件投递到任务队列后，通过管道的方式唤醒runLoop
 *      因为runLoop是阻塞在select或epoll_wait上，因此需要fd事件
 *          1）对于Window平台，轮询函数需要网络套接字才能唤醒，因此管道需要使用套接字模拟实现
 *          2）对于Linux平台，轮询函数使用文件描述符唤醒，因此使用管道即可
*/
EventPoller::Task::Ptr EventPoller::async(EventPoller::TaskIn&& task, bool may_sync) {
    if (may_sync && currentThread()) {
        task();
        return nullptr;
    }

    auto job = Task::create(std::move(task));
    {
        LOCK_GUARD(tasks_mutex_);
        tasks_.push_back(job);
    }

    //通过写管道唤醒轮询函数
    writePipe();
    return job;
}

EventPoller::Task::Ptr EventPoller::async_first(EventPoller::TaskIn&& task, bool may_sync) {
    if (may_sync && currentThread()) {
        task();
        return nullptr;
    }

    auto job = Task::create(std::move(task));
    {
        LOCK_GUARD(tasks_mutex_);
        tasks_.push_front(job);
    }

    //通过写管道唤醒轮询函数
    writePipe();
    return job;
}

EventPoller::EventPoller() :tasks_mutex_(true), event_records_mutex_(false) {

#if HAS_EPOLL
    epoll_fd_ = epoll_create(EPOLL_SIZE);
    if (epoll_fd_ == -1) {
        throw std::runtime_error((StrPrinter << "Failed to create epoll: " << get_uv_errmsg()));
    }

    //设置close-on-exec标志
    SockUtil::setCloOnExec(epoll_fd_);
#endif

    attachPipeEvent();
}

void EventPoller::writePipe() {
    static char buffer[1024] = { 0 };
    buffer[0] = 'w';
    pipe_.write(buffer, 1);
}

bool EventPoller::currentThread() const {
    return thread_ ? thread_->get_id() == std::this_thread::get_id() : true;
}

/**
 * 调度延迟任务
 * @return 返回最近定时器超时时间, 单位毫秒
*/
int64_t EventPoller::scheduleDelayTask() {
    return -1;
}

void EventPoller::onPipeEvent() {
    char buffer[1024];
    //pipe读事件，需要读完管道数据
    while (true) {
        int nread = pipe_.read(buffer, 1024);
        /**
         * 确保管道数据读取完成
        */
        if (nread > 0) continue;


        if (nread == -1 && (UV_EAGAIN == get_uv_error())) {
            //正常非阻塞I/O，没有数据的情形
            break;
        }

        /**
         * 其他情形： eof或其他出错,则重新创建pipe
        */
        detachEvent(pipe_.readFD());
        attachPipeEvent();
        break;
    }
 
    decltype(tasks_) tmp;
    {
        LOCK_GUARD(tasks_mutex_);
        if (tasks_.empty()) return;
        tasks_.swap(tmp);
    }

    for (auto& task : tmp) {
        if (task && (*task)) {

            try {
                (*task)();
            }
            catch (Exit&) {
                exit_ = true;

                /**
                * 线程内部不能join自己
                */
                /*if (thread_ && thread_->joinable()) {
                    thread_->join();
                    thread_ = nullptr;
                }*/

            }
            catch (...) {
                //其他异常忽略
                continue;
            }
            
        }
    }
}

void EventPoller::attachPipeEvent() {
    /**
     * 设置管道文件描述符的io属性
     * 此处管道文件描述符创建成功，否则早已经抛出异常了
     *
     * 使用轮询函数，需要将文件I/O设置为非阻塞
     *      因此，设置管道读通道为非阻塞
     * 对于管道写通道使用阻塞写
    */
    SockUtil::setNoBlocked(pipe_.readFD());
    SockUtil::setNoBlocked(pipe_.writeFD(), false);

    //注册管道读I/O事件，用于唤醒轮询线程, 并处理异步任务
    auto onEvent = [this](int) {
        onPipeEvent();
    };

    if (-1 == attachEvent(pipe_.readFD(), Event::kEventRead, std::move(onEvent))) {
        //注册事件失败，则抛出异常（因为影响正常功能了（导致程序无法退出）
        throw std::runtime_error(StrPrinter << "pipe failed to attchEvent");
    }
}

int EventPoller::attachEvent_l(int fd, EventRecord::Ptr eventRecord) {
    if (currentThread()) {
        int ret = 0;
        //fake lock
        LOCK_GUARD(event_records_mutex_);
        //处于同一个线程直接添加注册的事件记录
#if HAS_EPOLL
        struct epoll_event event = {0};
        event.events = TO_EPOLL_EVENT(eventRecord->events_);
        event.data.fd = fd;
        ret = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event);
        if (ret == -1) {
            WarnL << "Failed to epoll_ctl: " << get_uv_errmsg();
            return -1;
        }
#endif
        event_records_[fd] = eventRecord;
        return ret;
    }

    async([fd, eventRecord, this]()->void {
        attachEvent_l(fd, eventRecord);
    });
    return 0;

}

}//namespace util
}//namespace avc
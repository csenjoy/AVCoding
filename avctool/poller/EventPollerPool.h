#ifndef POLLER_EVENTPOLLERPOOL_H
#define POLLER_EVENTPOLLERPOOL_H

#include <vector>

#include "util/Util.h"
#include "poller/EventPoller.h"
#include "thread/TaskExecutorGetter.h"

namespace avc {
namespace util {

/**
 * EventPollerPool应该设计成为单例形式，以便异步对象（例如Socket,Timer等）
 * 能够获取到EventPoller，用以执行异步任务或注册异步I/O
 * 
 * EventPollerPool的职责：
 *      （1）负责创建多个EventPoller实例
 *      （2）提供获取EventPoller实例接口
 *       (3) 提供负载均衡： 保证异步对象分配均匀分配到各个EventPoller上
*/
class EventPollerPool : public TaskExecutorGetter,
                        Nocopyable {
public:
    using Ptr = std::shared_ptr<EventPollerPool>;
      
    AVC_STATIC_CREATOR(EventPollerPool)

    ~EventPollerPool();

    static EventPollerPool &instance();

    EventPoller::Ptr getEventPoller();
private:
    EventPollerPool();

    int addEventPoller(const std::string &name, int priority, bool cpuAffinity);
private:
    //static int eventpoller_count_ 
};//class EventPollerPool

}
}

#endif
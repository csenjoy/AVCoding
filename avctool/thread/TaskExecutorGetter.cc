#include "TaskExecutorGetter.h"

namespace avc {
namespace util {


TaskExecutor::Ptr TaskExecutorGetter::getTaskExecutor() {
    /**
    * 当负载都是0的时候，也需要负载均衡
    */
    if (pos_ >= task_executors_.size()) {
        pos_ = 0;
    }

    TaskExecutor::Ptr perferred = task_executors_[pos_];
    int minLoad = perferred->load();
    if (minLoad == 0) {
        //刚开始时，负载都为0，依次派发到没有个event poller
        pos_++;
        return perferred;
    }

    //查询其他负载更低的线程
    int load = 0;
    for (int index = 0; index < task_executors_.size(); ++index) {
        load = task_executors_[index]->load();
        if (load <= minLoad) {
            minLoad = load;
            perferred = task_executors_[index];
            pos_ = index;
        }
    }

    return perferred;
}

int TaskExecutorGetter::getTaskExecutorCount() const {
    return task_executors_.size();
}


}
}
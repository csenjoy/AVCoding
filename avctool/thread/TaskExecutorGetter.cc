#include "TaskExecutorGetter.h"

namespace avc {
namespace util {


TaskExecutor::Ptr TaskExecutorGetter::getTaskExecutor() {
    TaskExecutor::Ptr perferred = nullptr;
    int minLoad = 1000, load = 0;
    for (auto &executor : task_executors_) {
        if (executor) {
            load = executor->load();
            if (load <= minLoad) {
                minLoad = load;
                perferred = perferred;
            }
        }
    }
    return perferred;
}

int TaskExecutorGetter::getTaskExecutorCount() const {
    return task_executors_.size();
}


}
}
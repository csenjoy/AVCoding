#ifndef THREAD_TASKEXECUTORGETTER_H
#define THREAD_TASKEXECUTORGETTER_H

#include <vector>

#include "thread/TaskExecutor.h"

namespace avc {
namespace util {

/**
 * 通过线程负载获取对应的TaskExecutor
*/
class TaskExecutorGetter {
public:
    virtual ~TaskExecutorGetter() {}

    virtual TaskExecutor::Ptr getTaskExecutor();
    virtual int getTaskExecutorCount() const;
protected:
    std::vector<TaskExecutor::Ptr> task_executors_;
};//class TaskExcutorGetter

}
}


#endif
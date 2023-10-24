#include "TaskExecutorGetter.h"

namespace avc {
namespace util {


TaskExecutor::Ptr TaskExecutorGetter::getTaskExecutor() {
    /**
    * �����ض���0��ʱ��Ҳ��Ҫ���ؾ���
    */
    if (pos_ >= task_executors_.size()) {
        pos_ = 0;
    }

    TaskExecutor::Ptr perferred = task_executors_[pos_];
    int minLoad = perferred->load();
    if (minLoad == 0) {
        //�տ�ʼʱ�����ض�Ϊ0�������ɷ���û�и�event poller
        pos_++;
        return perferred;
    }

    //��ѯ�������ظ��͵��߳�
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
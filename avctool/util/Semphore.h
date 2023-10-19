#ifndef UTIL_SEMPHORE_H
#define UTIL_SEMPHORE_H

#include <mutex>
#include <condition_variable>

namespace avc {
namespace util {

/**
 * 信号量语义： 当信号量为0时，需要等待。
 *             不为0时，信号量减一，且不需要等待
*/
class Semphore {
public:
    Semphore(int n = 0) : nn_(n) {}

    /**
     * 当信号量为0时，wait操作会阻塞线程
    */
    void wait() {
        /**先加锁： 保证访问信号量nn_是线程安全的*/
        std::unique_lock<std::mutex> lock(mutex_);
        /**
         * 判断信号量为0, 线程被阻塞
         * while循环作用： 1）避免虚假唤醒（虚假唤醒后，发现没有资源继续进入wait)
         *                2) post时，n小于等待线程数量，此时也算是虚假唤醒的一种特例
         * */
        while(nn_ == 0) {
           cond_.wait(lock); /**wait操作会阻塞线程，并且对mutex_解锁*/
           //条件变量被唤醒：会对mutex_加锁
        }
        /**
         * 此处mutex_是加锁状态，所以nn_是线程安全的
        */
        --nn_;
    }

    /**
     * 增加信号量，唤醒等待的线程
     * @param n 增加信号量，如果n > 1时，唤醒所有等待线程
     *                      n == 1时，仅唤醒一条等待线程
     * @note 如果等待线程超过n的数量，此时被唤醒的线程会判断nn_ == 0条件
     *       如果此时nn_==0(即没有可用资源)，继续进入等待状态     
    */
    void post(unsigned n = 1) {
        std::unique_lock<std::mutex> lock(mutex_);
        nn_ += n;

        if (n == 1) {
            cond_.notify_one();
        }
        else {
            /**
             * notify_all会导致所有阻塞线程被唤醒，唤醒的线程对mutex进行加锁（其他唤醒的线程阻塞在对mutex的加锁上）
             * 然后执行--nn_后（wait函数返回）mutex被解锁。
             * 唤醒且阻塞在mutex的加锁上的线程获取到mutex锁，继续执行
            */
            cond_.notify_all();
        }
    }
private:
    int nn_;
    std::mutex mutex_;
    std::condition_variable cond_;
};//class Semphore

}
}

#endif
#ifndef UTIL_MUTEXWRAPPER_H
#define UTIL_MUTEXWRAPPER_H

#include <mutex>

namespace avc {
namespace util {

/**
 * 模板类MutexWrapper，实现无锁Mutex
*/
template<class _Mtx = std::recursive_mutex>
class MutexWrapper {
public:
    using mutex_type = typename _Mtx;

    MutexWrapper(bool enable) : enable_(enable) {}

    inline void lock() {
        if (enable_) {
            mutex_.lock();
        }
    }

    inline void unlock() {
        if (enable_) {
            mutex_.unlock();
        }
    }
private:
    _Mtx mutex_;
    bool enable_;
};//class MutexWrapper

#define LOCK_GUARD(mtx) std::lock_guard<decltype(mtx)> lock(mtx)
#define UNIQUE_LOCK(mtx) std::unique_lock<decltype(mtx)> lock(mtx)

}
}

#endif
#ifndef UTIL_UTIL_H
#define UTIL_UTIL_H

#include <string>
#include <stdarg.h>

#include <mutex>
#include <functional>

//#if __cplusplus > 201703
#define HAS_STRING_VIEW 1
//#endif

#if HAS_STRING_VIEW
#include <string_view>
#endif

#if defined(_MSC_VER)
#include <WinSock2.h> //struct timeval
#endif

#include "util/Semphore.h"
#include "util/Nocopyable.h"

namespace avc {
namespace util {

/**
 * 设置调用线程的名称
*/
void setThreadName(const char *name);
/**
 * 获取调用现场的名称 
*/
std::string getThreadName();

#if defined(_WIN32)
/**
 * Win32平台，没有提供gettimeofday函数
 * @brief ptz struct timezone
*/
int gettimeofday(struct timeval* tm, void* ptz);

/**
 * Win32平台，实现vasprintf
 *  动态方式申请格式化字符串的buffer,用户负责释放buf内存
*/
int vasprintf(char **buf, const char *fmt, va_list args);
int asprintf(char **buf, const char *fmt, ...);

#if HAS_STRING_VIEW
std::string filename(std::string_view path);
#else
std::string filename(const std::string& path);
#endif

#endif

/**
 * 模板类MutexWrapper，实现无锁Mutex
*/
template<class _Mtx = std::recursive_mutex>
class MutexWrapper {
public:
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

/**
 * 通过RAII，调用onConstructed与onDestructed
*/
class OnceToken {
public:
    using Task = std::function<void()>;
    template<class FUNC>
    OnceToken(const FUNC &onConstructed, Task onDestructed = nullptr) {
        onConstructed();
        onDestructed_ = std::move(onDestructed);
    }

    /**
     * 模板特化：构造时不需要调用处理函数，析构时可选传递函数对象
    */
    OnceToken(std::nullptr_t, Task onDestructed = nullptr) {
        onDestructed_ = std::move(onDestructed);
    }

    ~OnceToken() {
        if (onDestructed_) {
            onDestructed_();
        }
    }
private:
    Task onDestructed_;
};//class OnceToken

}
}

/**
 * 负责内存释放的helper宏
 * @param p: 使用delete释放内存，因为p可能是c++对象。
             使用free函数可能造成无法调用析构函数
*/
#define avc_free(p) if ((p)) delete(p)
#define avc_free_safe(p) if (*(p)) { delete(*(p)); *(p) = nullptr; }
//#define avc_free_array(p) if ((p)) delete [] (p)

#define avc_array_size(array) (size_t)(sizeof((array)) / sizeof((array)[0]))

#endif
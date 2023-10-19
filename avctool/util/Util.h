#ifndef UTIL_UTIL_H
#define UTIL_UTIL_H

#include <string>
#include <stdarg.h>

#include <mutex>
#include <functional>
#include <sstream>

//#if __cplusplus > 201703
#define HAS_STRING_VIEW 0
//#endif

#if HAS_STRING_VIEW
#include <string_view>
#endif

#if defined(_MSC_VER)
#include <WinSock2.h> //struct timeval
#endif

#include "util/Semphore.h"
#include "util/Nocopyable.h"
#include "util/MutexWrapper.h"
#include "util/OnceToken.h"
#include "util/StrPrinter.h"

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
void usleep(int micro_seconds);
void sleep(int second);
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
#endif//#if HAS_STRING_VIEW

#endif //#if defined(_WIN32)

/**
 * @brief 获取Epoch(1970-01-01 00:00:00 UTC)至今的毫秒数
 * @param systemTime 是否为系统时间；否则为程序启动时间
 * 
 *        通过启动一个线程方式，去更新当前系统时间和程序启动时间
 *        系统时间伴随着用户修改系统本地时间而变化；而程序启动时间不随用户修改系统本地时间的变化而变化
 *        因此可以认为：
 *             系统时间（可回退）；程序启动时间（不可回退，一直递增）
*/
uint64_t getCurrentMillisecond(bool systemTime = false);
uint64_t getCurrentMicrosecond(bool systemTime = false);


#ifndef bzero
#define bzero(ptr,size)  memset((ptr),0,(size));
#endif //bzero

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

#define AVC_STATIC_CREATOR(ClassType) template<class ...ARGS> \
                               static ClassType::Ptr create(ARGS &&...args) { \
                                    return ClassType::Ptr(new ClassType(std::forward<ARGS>(args)...)); \
                               }

#endif
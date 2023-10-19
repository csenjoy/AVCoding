#ifndef UTIL_ONCETOKEN_H
#define UTIL_ONCETOKEN_H

#include <functional>

namespace avc {
namespace util {

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

}//namespace util
}//namespace avc

#endif
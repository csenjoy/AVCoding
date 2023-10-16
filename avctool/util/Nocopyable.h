#ifndef UTIL_NOCOPYABLE_H
#define UTIL_NOCOPYABLE_H

namespace avc {
namespace util {

class Nocopyable {
public:
    Nocopyable() {}
    ~Nocopyable() {}
    //禁止拷贝构造
    Nocopyable(const Nocopyable &) = delete;
    Nocopyable &operator=(const Nocopyable &) = delete;
    //禁止移动构造
    Nocopyable(Nocopyable &&) = delete;
    Nocopyable &operator=(Nocopyable &&) = delete;
};//class Nocopyable

}
}

#endif
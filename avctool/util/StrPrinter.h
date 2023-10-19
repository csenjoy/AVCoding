#ifndef UTIL_STRPRINTER_H
#define UTIL_STRPRINTER_H

#include <string>
#include <sstream>

namespace avc {
namespace util {

/**
 * @brief 使用C++流式格式字符串
*/
class _StrPrinter : public std::string {
public:
    _StrPrinter() {}
    
    template<class T>
    _StrPrinter &operator<<(T &&data) {
        oss_ << std::forward<T>(data);
        /**
         * 每次输入一个参数的时候都会覆盖当前string内存
         *      此处存在性能开销
        */
        this->std::string::operator=(oss_.str());
        return *this;
    }

    std::string operator<<(std::ostream &(*f)(std::ostream &)) const {
        return *this;
    }
private:
    std::ostringstream oss_;
};//class _StrPrinter


}//namespace util
}//namespace avc

/**
 * 使用StrPrinter方便的使用C++流式格式化字符串
*/
#define StrPrinter avc::util::_StrPrinter()

#endif
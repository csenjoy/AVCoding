
#ifndef ERRNO_ERRNO_H
#define ERRNO_ERRNO_H

#include <exception>
#include <sstream>

namespace avc {
namespace util {

/**
 * @brief AvcCplxError: Complex错误描述
 * @note AvcCplxError，参考SRS项目中，关于错误类型定义
*/
class AvcCplxError;

/**
 * 定义错误类型
 *     使用int类型，简单描述错误：错误值与对应错误描述
 *     也可以使用自定义类，更加详细描述错误：错误值，错误描述，错误发生上下文环境等等
 *          例如，使用class AvcCplxError, typedef AvcCplxError * avc_errno_t
*/
//typedef int avc_error_t;

/**
 * 使用class AvcCplxError类描述错误的详细信息
*/
typedef AvcCplxError* avc_error_t;


/**
 * str_error_code_func： 错误描述函数指针
*/
typedef const char *(*str_error_code_func)(int ec);

/**
 * 错误值描述函数： 将错误值转为错误描述
 * @note 非线程安全函数
*/
extern const char *str_error_code(int ec);

/**
 *-------------------------------------------------------- 
*/

/**
 * 注册错误描述函数： 各个模块定义自己错误描述函数，并注册到errno模块
 *                   方便地提供同一StrErrno转为各个种错误值
 *                                
*/
extern void register_str_error_code(int ec_start, int ec_count, str_error_code_func func);


/**
 * 描述详细错误
*/
class AvcCplxError {
public:
    /**
     * 创建AvcCplxError
     * @param fmt 格式化字符串，提供错误日志信息
    */
    static AvcCplxError *create(const char *file, const char *func, int line, int ec, const char *fmt, ...);
    static AvcCplxError *wrap(const char *file, const char *func, int line, AvcCplxError *err, const char *fmt, ...);
    static AvcCplxError *copy(AvcCplxError *from);
    /**
     * 访问整个错误栈
    */
    static std::string summary(AvcCplxError *err);
    /**
     * 访问错误值
    */
    static int error_code(AvcCplxError *err);
    /**
     * 访问错误值对应的错误描述
    */
    static std::string error_code_longstr(AvcCplxError *err);
public:
    virtual ~AvcCplxError();
private:
    /**
     * 声明私有构造函数，需要通过上述静态函数创建AvcCplxError
    */
    AvcCplxError(const char *file, const char *func, int line, int ec, std::string &&message);
    AvcCplxError(const char* file, const char* func, int line, AvcCplxError *err, std::string&& message);

    //支持拷贝错误栈
    AvcCplxError(const AvcCplxError& rhs) {
      file_ = rhs.file_;
      func_ = rhs.func_;
      line_ = rhs.line_;
      ec_ = rhs.ec_;
      message_ = rhs.message_;
      //调用copy函数拷贝，递归拷贝所有wrap
      wrap_ = copy(rhs.wrap_);
    }

    std::string summary();
private:
    const char *file_ = nullptr;
    const char *func_ = nullptr;
    int line_ = -1;
    int ec_ = -1;
    /**
     * fmt格式化字符串
    */
    std::string message_;

    /**
     * wrap记录错误发生的栈信息
    */
    AvcCplxError *wrap_ = nullptr;

    std::string summary_;
};//class AvcCplxError

/**
 * 通过宏定义嵌套方式定义错误值与其对应的错误描述
*/
/**
 * 系统相关的错误值映射
*/
#define AVC_ERRNO_MAP_SYSTEM(XX)    \
    XX(ERRNO_SYSTEM_FILE_OPEN, 1042, "FileOpen", "Failed to open file") \
    XX(ERRNO_SYSTEM_FILE_CLOSE, 1043, "FileClose", "Failed to close file")

/**
 * 生成错误值
*/
#define AVC_ERRNO_GEN(n, v, m, s)   n = v,

//定义错误值枚举
enum AvcErrorCode {
    ERRNO_UNKOWN = -1,
    ERRNO_SUCCESS = 0,

    AVC_ERRNO_MAP_SYSTEM(AVC_ERRNO_GEN)
};//enum AvcErrorCode

} //namespace avc
} //namespace util


/**
 * 错误类型可以隐藏错误的详细描述，例如：错误值，错误描述，错误发生上下文环境
 * 通过宏去访问这些属性
*/
/**
 * 访问错误值
*/
#define avc_error_code(err)   avc::util::AvcCplxError::error_code((err))
/**
 * 访问错误描述
*/
#define avc_error_codestr(err)  avc::util::AvcCplxError::error_code_longstr((err))

/**
 * 定义错误类型
*/
#define avc_error_sucess 0
#define avc_error_new(ec, fmt, ...)   avc::util::AvcCplxError::create(__FILE__, __FUNCTION__, __LINE__, ec, fmt, __VA_ARGS__)
#define avc_error_wrap(err, fmt, ...) avc::util::AvcCplxError::wrap(__FILE__, __FUNCTION__, __LINE__, err, fmt, __VA_ARGS__)
#define avc_error_copy(err) avc::util::AvcCplxError::copy(err)
#define avc_error_message(err) 
#define avc_error_summary(err) avc::util::AvcCplxError::summary(err)

#endif
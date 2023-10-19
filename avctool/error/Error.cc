
#include "Error.h"

#include <unordered_map>

#include "util/Util.h"

namespace avc {
namespace util {


/**
 * 定义错误值对应的描述信息
*/
#define AVC_STRERRNO_GEN(n, v, m, s) {(AvcErrorCode)(n), m, s},

static struct {
    AvcErrorCode ec;
    const char *name;
    const char  *desc;
}_avc_strerrno_tab [] = {
    {AvcErrorCode::ERRNO_UNKOWN, "Unkown", "Unkown"},
    {AvcErrorCode::ERRNO_SUCCESS, "Sucess", "Success"},
    AVC_ERRNO_MAP_SYSTEM(AVC_STRERRNO_GEN)
};//_avc_strerrno_tab

/**
 * 通过OnceToken，在主线程中初始化错误描述信息
 * 修复在str_error_code函数中lazy构造可能会出现线程安全问题
*/
static std::unordered_map<int, const char *> _desc_map;
static OnceToken _error_initialize(
    [&]()->void {
        if (_desc_map.empty()) {
            for (int i = 0; i < avc_array_size(_avc_strerrno_tab); ++i) {
                _desc_map[_avc_strerrno_tab[i].ec] = _avc_strerrno_tab[i].desc;
            }
        }
    }
);

const char* str_error_code(int ec) {
    //注释代码，由于线程不安全，通过OnceToken初始化错误值描述
#if 0
    //通过hash_map快速映射ec与对应错误描述
    static std::unordered_map<int, const char *> desc_map;

    //@todo 此处使用static方式lazy构造，可能会出现线程安全问题
    //构造错误描述字符串缓存
    if (desc_map.empty()) {
        for (int i = 0; i < avc_array_size(_avc_strerrno_tab); ++i) {
            desc_map[_avc_strerrno_tab[i].ec] = _avc_strerrno_tab[i].desc;
        }
    }
#endif
    auto found = _desc_map.find(ec);
    if (found != _desc_map.end()) {
        return found->second;
    }
    return _desc_map[-1];
}

AvcCplxError* AvcCplxError::create(const char* file, const char* func, int line, int ec, const char* fmt, ...) {
  //格式字符串
  char* pmessage = nullptr;
  va_list ap;
  va_start(ap, fmt);
  int nwritten = vasprintf(&pmessage, fmt, ap);
  va_end(ap);

  std::string message;
  if (pmessage && nwritten > 0) {
    message = std::move(std::string(pmessage, nwritten));
    avc_free(pmessage);
  }

  AvcCplxError* cplxError = new AvcCplxError(file, func, line, ec, std::move(message));
  return cplxError;
}

AvcCplxError* AvcCplxError::wrap(const char* file, const char* func, int line, AvcCplxError* err, const char* fmt, ...) {
  //格式字符串
  char* pmessage = nullptr;
  va_list ap;
  va_start(ap, fmt);
  int nwritten = vasprintf(&pmessage, fmt, ap);
  va_end(ap);

  std::string message;
  if (pmessage && nwritten > 0) {
    message = std::move(std::string(pmessage, nwritten));
    avc_free(pmessage);
  }

  AvcCplxError* cplxError = new AvcCplxError(file, func, line, err, std::move(message));
  return cplxError;
}

AvcCplxError* AvcCplxError::copy(AvcCplxError* from) {
  //from为nullptr时，无需拷贝
  if (from == nullptr) return nullptr;

  //创建拷贝对象
  //AvcCplxError* cplxError = new AvcCplxError(from->file_, from->func_, line, err, std::move(message));
  //return cplxError;

  /**
   * 调用拷贝构造函数
  */
  AvcCplxError* cplxError = new AvcCplxError(*from);
  return cplxError;
}

std::string AvcCplxError::summary(AvcCplxError* err) {
  return err ? err->summary() : "Success";
}

int AvcCplxError::error_code(AvcCplxError* err) {
    if (err) return err->ec_;
    return AvcErrorCode::ERRNO_SUCCESS;
}

std::string AvcCplxError::error_code_longstr(AvcCplxError* err) {
    std::string strerr;
    strerr = str_error_code(error_code(err));
    return std::move(strerr);
}

AvcCplxError::~AvcCplxError() {
  /**
   * 存在嵌套（通过wrap方式创建AvcCplxError，则会出现此类情况)
  */
  if (wrap_) {
    avc_free_safe(&wrap_);
  }
}

AvcCplxError::AvcCplxError(const char* file, const char* func, int line, int ec, std::string&& message) 
  : file_(file), func_(func), line_(line), ec_(ec), message_(std::move(message)){

}

AvcCplxError::AvcCplxError(const char* file, const char* func, int line, AvcCplxError *err, std::string&& message)
  : file_(file), func_(func), line_(line), ec_(err ? error_code(err) : -1), message_(std::move(message)), wrap_(err) {

}


std::string AvcCplxError::summary() {
    if (summary_.empty()) {
        std::ostringstream oss;
        oss << "code=(" << str_error_code(ec_) << ")" << std::endl;
        AvcCplxError *next = this;

        int loop = 0;
        while(next) {
            for (int tab = 0; tab < loop; ++tab) {
                oss << "    ";
            }

            oss << "[" << filename(next->file_) << " " << next->func_ << ":" << next->line_ << "]: " << next->message_ << std::endl;
            next = next->wrap_;

            loop++;
        }

        summary_ = oss.str();
    }

    return summary_;
}

} //namespace avc
} //namespace util
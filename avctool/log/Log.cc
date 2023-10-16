#include "log.h"

namespace avc {
namespace util {

static const char *szLogLevel[] = {
    "Trace",
    "Debug",
    "Warn",
    "Error",
    "Fatal",
    ""
};//szLogLevel

const char *strLogLevel(unsigned level) {
    if (level > LogLevel::kLogLevelFatal) {
        level = LogLevel::kLogLevelMax;
    }
    return szLogLevel[level];
}

LogContext::LogContext() {

}

LogContext::LogContext(int level, const char *file, const char* func, int line) {
    env_.level = level;
    env_.file = file;
    env_.func = func;
    env_.line = line;
    //env_.time = util::gettimeofday();
    gettimeofday(&env_.tv, nullptr);
    env_.threadName = getThreadName();
}

Logger& Logger::instance() {
    static Logger s_instance;
    return s_instance;
}


std::string LogChannel::formatTime(const timeval &tv) {
    //timeval中tv_sec字段代表time_t（日历时间）
    time_t sec = tv.tv_sec;
    auto tm = localtime(&sec);

    std::string format;
    format.resize(128);
    if (tm) {
        int nwritten = snprintf((char *)format.data(), 128, "%d/%02d/%02d %02d:%02d:%02d.%03d",
            1900 + tm->tm_year,
            1 + tm->tm_mon,
            tm->tm_mday,
            tm->tm_hour,
            tm->tm_min,
            tm->tm_sec,
            (int)(tv.tv_usec / 1000));
        //format[nwritten] = '\0';
        format.resize(nwritten);
    }
    return std::move(format);
}

/**
 * PrintV函数参数，展开一个fmt参数
*/
void LogContextCaptureWrapper::PrintLog(Logger& logger, int level, const char* file, const char* func, int line, const char* fmt, ...) {
    //格式日志正文字符串
    va_list args;
    va_start(args, fmt);
    PrintLogV(logger, level, file, func, line, fmt, args);
    va_end(args);
}

void LogContextCaptureWrapper::PrintLogV(Logger& logger, int level, const char* file, const char* func, int line, const char* fmt, va_list args) {
    //先捕获日志输入上下环境
    LogContextCapture capture(logger, level, file, func, line);

    //通过vasprintf函数，格式化字符串
    char* content = nullptr;
    if (vasprintf(&content, fmt, args) > 0 && content) {
      capture << content;
      free(content);
    }
}

}//namespace util
}
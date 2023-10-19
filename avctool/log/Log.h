#ifndef LOG_LOG_H
#define LOG_LOG_H

#include <memory>//LogContext在模块之间传递，所以需要使用shared_ptr
#include <string>
#include <vector>
#include <thread>
#include <sstream>//使用std::ostringstream

#include <iostream>//控制台日志通道

#include "util/Util.h" //平台依赖判断和帮助函数与类

namespace avc {
namespace util {

enum LogLevel {
    kLogLevelTrace,
    kLogLevelDebug,
    kLogLevelWarn,
    kLogLevelErr,
    kLogLevelFatal,
    kLogLevelMax
};//enum LogLevel

extern const char *strLogLevel(unsigned level);

//前置声明
/**
 * @brief 日志上下文：包含日志输出上下文以及日志正文
*/
struct LogContext;

/**
 * 用户调用日志接口时，使用LogContextCapture负责捕获日志上下文环境以及日志正文
 *      日志正文：通过operator<<流式方式输入，并且以std::endl表明结束输入（类似行缓冲逻辑）
 *      日志写入：1) 当LogContextCapture析构时，调用Logger接口写入日志
 *               2) 用户输入换行时，调用Logger接口，立即写入日志 
*/
struct LogContextCapture;

class Logger;


/**
 * LogContext日志上下文，负责C++流式格式化输入日志正文参数
 *    通过std::ostringstream，实现operator<<流式输入
*/
struct LogContext : public std::ostringstream
{
    using Ptr = std::shared_ptr<LogContext>;
    using Content = std::string;
    
    LogContext();
    LogContext(int level, const char *file, const char* func, int line);

    template<typename ...ARGS>
    static Ptr create(ARGS&& ...args) {
      //对每个arg参数进行std::forward展开
      return std::make_shared<LogContext>(std::forward<ARGS>(args)...);
    }

    /**
     * 日志上下文环境包括：
     *   日志输出点：所在函数，所在行数，线程，日志打印时间
     *  
    */
    struct Env  {
      int level; //日志级别
      const char *file;//所在文件
      const char* func;//所在函数
      int line;//所在行数
      /**
      * 日志打印时间，精度要求毫秒，因此使用gettimeofday函数即可
      *   timeval保存秒以及微秒
      */
      struct timeval tv;//日志打印时间
      std::string threadName;//线程名称
    };//struct Env

    const Content& content() {
      if (gotContent_) {
        return content_;
      }
      content_ = str();
      return content_;
    }

    /**
     * 日志的上下文环境
    */
    Env env_;
    /**
     * 已格式化的日志正文
    */
    Content content_;
    bool gotContent_ = false;
};//struct LogContext

/**
 * @brief 写日志接口
*/
struct LogWriter {
 using Ptr = std::shared_ptr<LogWriter>;

 virtual ~LogWriter() {}
 virtual void writeLog(LogContext::Ptr, Logger &) = 0;
};//struct LogWriter

/**
 * 日志通道： 每个日志通道格式化方式不同
*/
class LogChannel {
public:
    using Ptr = std::shared_ptr<LogChannel>;

    virtual ~LogChannel() {}

    virtual void writeLog(LogContext::Ptr log) = 0;

    void setLevel(int level) {
      level_ = level;
    }
protected:
    virtual void format(LogContext::Ptr log, std::ostream &ost) {
        if (log->env_.level < level_) return;

        //输出前置格式
        ost << "[" << formatTime(log->env_.tv) << "]";
        ost << "[" << log->env_.func << ":" << log->env_.line
          << " " << log->env_.threadName << "][" << strLogLevel(log->env_.level) << "]: ";
        //输出日志正文
        ost << log->content();
    }
private:
    static std::string formatTime(const timeval &time);
private:
    int level_ = LogLevel::kLogLevelTrace;//日志级别
};//class LogChannel

/**
 * 控制台日志输出通道
 * 
*/
class LogChannelConsole : public LogChannel {
public:
  LogChannelConsole() {}
  virtual ~LogChannelConsole()  {}

  void writeLog(LogContext::Ptr log) override {
    format(log, std::cout);
    //标准输出I/O，控制台是行缓冲，通过std::endl冲洗缓冲区
    std::cout << std::endl;
  }
};//class LogChannelConsole

/**
 * 文件日志输出通道
*/
class LogChannelFile : public LogChannel {

};//class LogChannelFile

/**
 * Logger设计成全局静态变量
*/
class Logger {
public:
    static Logger &instance();

    /**
     * 添加日志输出通道
    */
    void addLogChannel(LogChannel::Ptr logChannel) {
      channels_.emplace_back(logChannel);
    }
 
    /**
     * 设置日志writer,默认情况同步写日志
     *      一般用于设置异步日志Writer
    */
    void setWriter(LogWriter::Ptr writer) {
      writer_ = writer;
    }

    void writeLog(LogContext::Ptr log) {
        if (writer_) {
            return writer_->writeLog(log, *this);
        }
        writeLogToChannels(log);
    }
    
    void writeLogToChannels(LogContext::Ptr log) {
      for (auto& node : channels_) {
        node->writeLog(log);
      }
    }
private:
    LogWriter::Ptr writer_;

    std::vector<LogChannel::Ptr> channels_;
};//class Logger

class AsyncLogWriter : public LogWriter {
public:
    AsyncLogWriter() {
      exit_ = false;
      thread_ = std::make_shared<std::thread>([this]()->void { this->run(); });
    }

    ~AsyncLogWriter() {
        exit();
    }
    
    /**
     * @brief writeLog时，指定依赖的Logger
    */
    void writeLog(LogContext::Ptr log, Logger &logger) override {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            pendding_.push_back(std::make_pair(log, &logger));
        }
        semphore_.post();
    }
private:
    void run() {
        util::setThreadName("AsyncLogWriter");
        while(!exit_) {
            semphore_.wait();

            decltype(pendding_) tmp;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                if (pendding_.empty()) continue;

                pendding_.swap(tmp);
            }

            for (auto &node : tmp) {
                node.second->writeLogToChannels(node.first);
            }
        }
    }

    void exit() {
      /**
       * exit_变量有多线程访问的情形。
       *     冲突发生场景：其他线程调用exit退出, 对run线程退出条件判断没什么影响
       *     因此不需要加锁
      */
      exit_ = true;
      semphore_.post();

      thread_->join();
      thread_.reset();
    }
private:
    std::mutex mutex_;
    std::vector<std::pair<LogContext::Ptr, Logger *>> pendding_;

    util::Semphore semphore_;
    bool exit_ = false;//exit_变量需要加锁吗 ?
    std::shared_ptr<std::thread> thread_;
};//class AsyncLogWriter

/***/
struct LogContextCapture { 
    LogContextCapture(Logger &logger, int level, const char *file, const char *func, int line) : logger_(logger) {
        context_ = LogContext::create(level, file, func, line);
    }
    /**
     * 通过栈变量，捕获日志上下文。当变量析构时，触发写入日志
    */
    ~LogContextCapture() {
        //析构时，输入std::endl，触发日志写入
        *this << std::endl;
    }

    /**
     * 用户输入换行符，也会触发立即写入日志
     * @param f operator<<操作参数，是函数指针。
     * @note 重载的参数是，输出流的操作子函数，这样的话出了std::endl，其他操作子也会触发
     *                    但是影响不大，因为context_写完后会释放
    */
    LogContextCapture &operator<<(std::ostream &(*f)(std::ostream &)) {
        /**
         * 需要判断context_是否存在，避免日志被重复写入
        */
        if (context_) {
            logger_.writeLog(context_);
            //写入日志后，context_释放，避免重复写入日志
            context_.reset();
        }
        return *this;    
    }
    

    template<class T>
    LogContextCapture &operator<<(T &&data) {
        if (context_) {
            (*context_) << std::forward<T>(data);
        }
        return *this;
    }

    Logger &logger_;
    LogContext::Ptr context_;
};

/**
 * C接口适配层，处理C可变参数输入得到格式化字符串后，通过LogContextCapture写入日志
*/
struct LogContextCaptureWrapper {

    /**
      * PrintLog函数参数，展开一个fmt参数
    */
    static void PrintLog(Logger& logger, int level, const char* file, const char* func, int line, const char* fmt, ...);
    static void PrintLogV(Logger& logger, int level, const char* file, const char* func, int line, const char* fmt, va_list args);
};

}// namespace util
} // namespace avc

/**
 * C++流式格式日志输入形式
*/
#define WriteL(level) avc::util::LogContextCapture(avc::util::Logger::instance(), (level), __FILE__, __FUNCTION__, __LINE__)
#define TraceL WriteL(avc::util::LogLevel::kLogLevelTrace)
#define DebugL WriteL(avc::util::LogLevel::kLogLevelDebug)
#define InfoL  WriteL(avc::util::LogLevel::kLogLevelInfo)
#define WarnL  WriteL(avc::util::LogLevel::kLogLevelWarn)
/**
 * C格式化日志输入形式
*/
#define PrintL(level, ...) avc::util::LogContextCaptureWrapper::PrintLog(avc::util::Logger::instance(), (level), __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);
#define PrintT(...) PrintL(avc::util::LogLevel::kLogLevelTrace, __VA_ARGS__)
#define PrintD(...) PrintL(avc::util::LogLevel::kLogLevelDebug, __VA_ARGS__)

#endif
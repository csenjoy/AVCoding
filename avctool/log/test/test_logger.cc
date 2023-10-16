#include <iostream>


#include "log/log.h"

int main(int argc, char **argv) {
    using namespace avc::util;
    setThreadName("MainThread");

#if 1
    {
        //测试AsyncLogWriter
        AsyncLogWriter asyncWritter;
        auto log = LogContext::create(1, __FILE__, __FUNCTION__, __LINE__);
        (*log) << "this is a message";
        asyncWritter.writeLog(log, Logger::instance());
    }
#endif

    {
        /**
         * 测试使用父类指针指针管理子类实例对象
        */
        LogWriter::Ptr(new AsyncLogWriter());
    }
    
    {
      //测试日志上下文捕获LogContextCapture
      LogContextCapture capture(Logger::instance(), 1, __FILE__, __FUNCTION__, __LINE__);
      //测试行缓冲立即写入日志
      capture << "This is a message." << std::endl;

      //测试其他操作子（例如std::flush）也会触发，但触发不会导致日志被重复写入
      LogContextCapture capture1(Logger::instance(), 1, __FILE__, __FUNCTION__, __LINE__);
      capture1 << "This is a message flush immediately." << std::endl << std::flush;

      //测试capture1，行缓冲后，无法继续使用
      capture1 << "This is a message cant input into logger";

      //此处capture1与capture2析构,由于日志早已写入，因此是无效的冲洗
    }

    //Logger设置异步日志Writer，如果不设置, 默认是同步写日志
    Logger::instance().setWriter(LogWriter::Ptr(new AsyncLogWriter()));
    //Logger增加日志输出通道: 控制台，文件
    Logger::instance().addLogChannel(LogChannel::Ptr(new LogChannelConsole()));

    std::string input;
    do {
      std::cin >> input;

      if (input == "w") {
        TraceL << "This is a message";

        PrintT("This is a message");
        PrintD("This is a %s", "vmessage");
      }
      
    } while (input != "Q" && input != "q");

    getchar();
    return 0;
}
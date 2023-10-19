#include <iostream>
#include <string>

#include "log/Log.h"
#include "thread/ThreadPool.h"

using namespace avc::util;

int main(int argc, char** argv) {
  //使用日志库打印标准输出
  Logger::instance().addLogChannel(LogChannel::Ptr(new LogChannelConsole()));
  Logger::instance().setWriter(LogWriter::Ptr(new AsyncLogWriter()));
  
  auto threadPool = ThreadPool::create(5);

  std::string input;
  do {
      std::cin >> input;
      if (input == "post") {
          //创建任务
          auto job = threadPool->async([]() {
              DebugL << "async task is being executed.";
            });

          job = threadPool->async_first([]() {
              DebugL << "async_first task is being executed.";
             });
#if 0
          //取消任务
          job->cancel();//or (*job) = nullptr;
#endif
;      }
      else if (input == "send") {
          threadPool->sync([]() {
                DebugL << "sync task is being executed.";
            });

          threadPool->sync_first([]() {
              DebugL << "sync_first task is being executed.";
            });
      }
  } while (input != "Q" && input != "q");


  threadPool.reset();
  return 0;
}
#include <iostream>
#include <string>

#include "log/Log.h"
#include "poller/EventPoller.h"
#include "thread/ThreadLoadCounter.h"

using namespace avc::util;

class MyThread : public ThreadLoadCounter {
public:
    MyThread() {
        exit_ = false;
        thread_ = std::move(std::thread([this]()->void {
                   setThreadName("MyThread");
                   run();
            }));
    }
    ~MyThread() {
        exit_ = true;
        thread_.join();
    }
    
    void run() {
        int count = 0;
        while (!exit_) {
            count++;
            onSleep();
            usleep((count % 5 + 1) * 10000);
            onWakeup();
            
            //模拟执行耗时
            usleep((count % 5 + 1) * 1000);
        }
    }
private:
    bool exit_ = false;
    std::thread thread_;
};

int main(int argc, char** argv) {
  setThreadName("MainThread");
  //使用日志库打印标准输出
  Logger::instance().addLogChannel(LogChannel::Ptr(new LogChannelConsole()));
  Logger::instance().setWriter(LogWriter::Ptr(new AsyncLogWriter()));
  
  try {
      auto poller = EventPoller::create();
      poller->runLoop();

      MyThread myThread;
      /**
       * 每两秒钟，打印一次线程负载情况
      */
      poller->addDelayTask(2000, [&]()->uint64_t {
                DebugL << "MyThread load: " << myThread.load();
                return 2000;
          });

      std::string input;
      do {
          std::cin >> input;
          if (input == "post") {
          }
          else if (input == "send") {
          }
          else if (input == "shutdown") {
          }
          else if (input == "run") {

          }
          else if (input == "delay") {
          }
      } while (input != "Q" && input != "q");

  }
  catch (std::exception& ex) {
      WarnL << ex.what();
  }

  getchar();
}
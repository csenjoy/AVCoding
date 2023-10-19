
#include <iostream>
#include <string>

#include "log/Log.h"
#include "poller/EventPoller.h"

using namespace avc::util;

int main(int argc, char** argv) {
  setThreadName("MainThread");
  //使用日志库打印标准输出
  Logger::instance().addLogChannel(LogChannel::Ptr(new LogChannelConsole()));
  Logger::instance().setWriter(LogWriter::Ptr(new AsyncLogWriter()));
  
  try {

      {
          auto poller = EventPoller::create();
          poller->runLoop();
      }
      
      auto poller3 = EventPoller::create();
      poller3->runLoop();

      auto poller2 = EventPoller::create();
      poller2->runLoop();
      std::string input;
      do {
          std::cin >> input;
          if (input == "post") {
          }
          else if (input == "send") {
          }
          else if (input == "shutdown") {
              poller2 = nullptr;
          }
          else if (input == "run") {

          }
          else if (input == "delay") {
              poller2->addDelayTask(1000, []()->uint64_t {
                  DebugL << "delayTask callback.";
                  return 1000;
               });
          }
      } while (input != "Q" && input != "q");

  }
  catch (std::exception& ex) {
      WarnL << ex.what();
  }

  getchar();
   
}
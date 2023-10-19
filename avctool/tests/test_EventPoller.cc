
#include <iostream>
#include <string>

#include "log/Log.h"
#include "poller/EventPoller.h"

using namespace avc::util;

int main(int argc, char** argv) {
  //使用日志库打印标准输出
  Logger::instance().addLogChannel(LogChannel::Ptr(new LogChannelConsole()));
  Logger::instance().setWriter(LogWriter::Ptr(new AsyncLogWriter()));
  
  try {

      {
          auto poller = EventPoller::create();
          poller->runLoop();
      }
      
      auto poller = EventPoller::create();
      poller->runLoop();
      std::string input;
      do {
          std::cin >> input;
          if (input == "post") {
          }
          else if (input == "send") {
          }
          else if (input == "shutdown") {
              poller->shutdown();
          }
          else if (input == "run") {

          }
      } while (input != "Q" && input != "q");

  }
  catch (std::exception& ex) {
      WarnL << ex.what();
  }

  getchar();
   
}

#include <iostream>
#include <string>

#include "log/Log.h"
#include "poller/EventPollerPool.h"

using namespace avc::util;

int main(int argc, char** argv) {
  setThreadName("MainThread");
  //使用日志库打印标准输出
  Logger::instance().addLogChannel(LogChannel::Ptr(new LogChannelConsole()));
  Logger::instance().setWriter(LogWriter::Ptr(new AsyncLogWriter()));
  
  EventPollerPool::instance();

  try {


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
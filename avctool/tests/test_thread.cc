#include <iostream>
#include <string>

#include "log/Log.h"
#include "thread/Thread.h"

using namespace avc::util;

class MainLoop : public Thread {
public:
    MainLoop() : Thread("MainLoop") {

    }

    void run() override {
        DebugL << "run";
    }
 
};

int main(int argc, char** argv) {
  //使用日志库打印标准输出
  Logger::instance().addLogChannel(LogChannel::Ptr(new LogChannelConsole()));
  Logger::instance().setWriter(LogWriter::Ptr(new AsyncLogWriter()));
  

  MainLoop loop;

  std::string input;
  do {
      std::cin >> input;
      if (input == "post") {
;      }
      else if (input == "send") {

      }
  } while (input != "Q" && input != "q");

  return 0;
}
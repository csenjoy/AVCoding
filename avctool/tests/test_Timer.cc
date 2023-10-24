
#include <iostream>
#include <string>

#include "log/Log.h"
#include "timer/Timer.h"

using namespace avc::util;


class Printer {
public:
    Printer() {
        poller_ = EventPollerPool::instance().getEventPoller();

        int count = 1;
        for (int index = 0; index < count; ++index) {
            auto timer = Timer::create([this, index]()->void {
                    DebugL << "Timer callback: 0x" << this << "-" << index; 
                }, poller_);

            timer->start(1000);
            timer_.push_back(timer);
        }

    }
    ~Printer() {
        timer_.clear();
    }

private:
    EventPoller::Ptr poller_;
    std::vector<Timer::Ptr> timer_;
};



int main(int argc, char** argv) {
  setThreadName("MainThread");
  //使用日志库打印标准输出
  Logger::instance().addLogChannel(LogChannel::Ptr(new LogChannelConsole()));
  Logger::instance().setWriter(LogWriter::Ptr(new AsyncLogWriter()));
  
  {
      Printer printer;
      Printer printer2;
      Printer printer3;
      Printer printer4;
  }

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
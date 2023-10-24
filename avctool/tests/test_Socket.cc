
#include <iostream>
#include <string>

#include "log/Log.h"
#include "poller/EventPollerPool.h"
#include "network/Socket.h"

using namespace avc::util;

void ui_loop() {
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

void test_udp_server() {
    //创建Socket
    auto sockRecv = Socket::create();

    //socket->set
    std::weak_ptr<Socket> self = sockRecv;
    sockRecv->setOnRead([self](Buffer::Ptr buffer, struct sockaddr *addr, socklen_t len)->void {
        DebugL << "Recv form " << SockUtil::get_sockaddr_ip(addr) << "@"
               << SockUtil::get_sockaddr_port(addr) << ": " << buffer->data();

        //回显
        auto sock = self.lock();
        if (sock) {
            sock->send(buffer);
        }
    });
    sockRecv->bindUdpSocket(8888, "127.0.0.1");


    
    auto sockSend = Socket::create();
    
    /**
     * 注意： 不指定地址参数的时候，默认绑定的是ipv6
     *        因此会造成send函数发送失败（sockRecv中使用ipv4)
    */
    sockSend->bindUdpSocket(18888, "127.0.0.1");
    
    auto dst_addr = SockUtil::makeSockAddr("127.0.0.1", 8888);
    socklen_t len = SockUtil::get_sockaddr_len((struct sockaddr *)&dst_addr);

    int i = 0;
    while (true) {
        sockSend->send(std::to_string(i++), (struct sockaddr*)&dst_addr, len);
        sleep(1);
    }

    ui_loop();
}

int main(int argc, char** argv) {
  setThreadName("MainThread");
  //使用日志库打印标准输出
  Logger::instance().addLogChannel(LogChannel::Ptr(new LogChannelConsole()));
  Logger::instance().setWriter(LogWriter::Ptr(new AsyncLogWriter()));
  
//  EventPollerPool::instance();
  test_udp_server();
  getchar();
   
}
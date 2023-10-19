#include "PipeWrapper.h"

#include "network/SockUtil.h"
#include "util/Util.h"

#include "error/uv_errno.h"//使用uv库提供的错误类型以及错误描述

namespace avc {
namespace util {

#define CHECK_FD(fd) do {   \
                        if (fd == -1) { \
                        closeFD();  \
                        throw std::runtime_error(StrPrinter << "Create pipe failed at windows platform: " << get_uv_errmsg());  \
                        }   \
                    } while(0)  

#define CLOSE_FD(fd) do { \
                        if (fd != -1) {\
                            close(fd); \
                            fd = -1; \
                        } \
                    } while(0)
   

PipeWrapper::PipeWrapper() {
    reOpenFD();
}

PipeWrapper::~PipeWrapper() {
    closeFD();
}

int PipeWrapper::write(const void *buffer, int n) {
    int ret = 0;
    do {
#if defined(WIN32)
        ret = send(fd_[1], (char *)buffer, n, 0);
#else
        ret = ::write(fd_[1], buffer, n);
#endif
    } while (ret == -1 && UV_EINTR == get_uv_error());
    return ret;
}

int PipeWrapper::read(void *buffer, int n) {
    int ret = 0;
    do {
#if defined(WIN32)
        ret = recv(fd_[0], (char *)buffer, n, 0);
#else 
        ret = ::read(fd_[0], buffer, n);
#endif
    } while(ret == -1 && UV_EINTR == get_uv_error());
    return ret;
}

void PipeWrapper::closeFD()
{
    CLOSE_FD(fd_[0]);
    CLOSE_FD(fd_[1]);
}

void PipeWrapper::reOpenFD() {
    closeFD();
#if defined(WIN32)
    /**
     * 为了确保Win32平台的轮询函数（select或完成端口)能够处理管道文件描述符
     * 所以使用tcp链接模拟实现管道逻辑
    */
    const char *localIp = SockUtil::support_ipv6() ? "::1" : "127.0.0.1";
    int listenFd = SockUtil::listen(0, localIp);
    //如果创建socket失败，直接抛出异常
    CHECK_FD(listenFd);

    auto localPort = SockUtil::get_local_port(listenFd);

    fd_[1] = SockUtil::connect(localIp, localPort, false);
    CHECK_FD(fd_[1]);
    fd_[0] = SockUtil::accept(listenFd, nullptr, nullptr);
    CHECK_FD(fd_[0]);
    //tcp链接作为管道时，不需要延迟算法
    SockUtil::setNoDelay(fd_[0]);
    SockUtil::setNoDelay(fd_[1]);

    //建立起tcp链接后，不再需要使用listenFd
    CLOSE_FD(listenFd);
#else
    //linux平台使用pipe函数创建管道即可
    if (-1 == pipe(fd_)) {
        //创建失败抛出异常
        throw std::runtime_error(StrPrinter << "Create pipe failed at non-win32 platform: " << get_uv_errmsg());
    }
#endif
    //管道写通道，设置为非阻塞
    SockUtil::setNoBlocked(fd_[0], true);
    //管道读通道设置为阻塞，确保写操作能完成
    SockUtil::setNoBlocked(fd_[1], false);

    //设置文件描述符的close-on-exec标志
    SockUtil::setCloOnExec(fd_[0]);
    SockUtil::setCloOnExec(fd_[1]);
}


}
}
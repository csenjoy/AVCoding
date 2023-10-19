#ifndef POLLER_SELECTWRAPPER_H
#define POLLER_SELECTWRAPPER_H

#include "util/Util.h"
#include "network/SockUtil.h"

namespace avc {
namespace util {

struct FdSet{
    FdSet();
    ~FdSet();
    bool hasFd(int fd);
    void addFd(int fd);

    void *fd_set_ = nullptr;
};//struct FdSet

int Select(int maxFd, FdSet *read, FdSet *write, FdSet *except, struct timeval *timeout);

}//namespace util
}//namespace avc

#endif
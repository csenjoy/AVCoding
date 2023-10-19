#include "SelectWrapper.h"

#include "util/Util.h"

namespace avc {
namespace util {

#define D_P(Type, Pointer)   Type *d = static_cast<Type *>((Pointer))

FdSet::FdSet(){
    auto set = (fd_set *)malloc(sizeof(fd_set));
    FD_ZERO(set);
    fd_set_ = set;    
}

FdSet::~FdSet() {
    //D_P(FD_SET, fd_set_);
    free(fd_set_);
    fd_set_ = nullptr;
}

bool FdSet::hasFd(int fd) {
    return FD_ISSET(fd, fd_set_);
}

void FdSet::addFd(int fd) {
    FD_SET(fd, fd_set_);
}

int Select(int maxFd, FdSet *read, FdSet *write, FdSet *except, timeval *timeout) {
    return ::select(maxFd, (fd_set*)read->fd_set_, (fd_set*)write->fd_set_, (fd_set*)except->fd_set_, timeout);
}

}
}
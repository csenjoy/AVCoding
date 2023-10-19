#ifndef POLLER_EPOLLWRAPPER_H
#define POLLER_EPOLLWRAPPER_H

#if defined(WIN32)
//#include "wepoll"
#include "wepoll/wepoll.h"

#else 
#include <sys/epoll.h>
#endif


namespace avc {
namespace util {

#if defined(WIN32)
/**
 * wepoll提供的epoll_create函数返回值与linux平台的epoll_create不一致
 * 
 * 增加一层wrapper，使接口与linux平台接口具有一致性
*/
int epoll_create(int size);
int epoll_ctl(int epollfd, int op, int fd, struct epoll_event *event);
int epoll_wait(int epollfd, struct epoll_event *events, int maxevents, int timeout);
int epoll_close(int epollfd);
#else
#endif

}
}


#endif
#include "EpollWrapper.h"

#include <map>
#include <mutex>

#include "util/Util.h"

namespace avc {
namespace util {

#if defined(WIN32)
/**
 * wepoll提供的epoll_create函数返回值与linux平台的epoll_create不一致
 * 
 * 增加一层wrapper，使接口与linux平台接口具有一致性
*/

static std::map<int, HANDLE> s_epollfds;
static int s_fd = 0;
static std::mutex s_mutex;

static inline int get_fd(HANDLE hnd) {
    LOCK_GUARD(s_mutex);

    //auto node = s_epollfds.find()
    auto node = s_epollfds.rbegin();
    int fd_min = s_epollfds.empty() ? 0 : s_epollfds.begin()->first;
    int fd_max = s_epollfds.empty() ? 0 : s_epollfds.rbegin()->first;
    int fd = max(fd_min, fd_max) + 1;

    s_epollfds[fd] = hnd;
    s_fd = fd;
    return fd;
}

static inline HANDLE get_hnd(int fd) {
    LOCK_GUARD(s_mutex);
    auto node = s_epollfds.find(fd);
    if (node == s_epollfds.end()) {
        return NULL;
    }
    return node->second;
}

static inline HANDLE remove_fd(int fd) {
    HANDLE hnd = NULL;
    LOCK_GUARD(s_mutex);
    auto node = s_epollfds.find(fd);
    if (node != s_epollfds.end()) {
        hnd = node->second;
        s_epollfds.erase(node);
    }
    
    return hnd;
}

int epoll_create(int size) {
    HANDLE hnd = ::epoll_create(size);
    if (hnd == NULL) {
        return -1;
    }
    return get_fd(hnd);
}

int epoll_ctl(int epollfd, int op, int fd, struct epoll_event* event) {
    HANDLE hnd = get_hnd(epollfd);
    if (hnd == NULL) return -1;

    return ::epoll_ctl(hnd, op, fd, event);
}

int epoll_wait(int epollfd, struct epoll_event* events, int maxevents, int timeout) {
    HANDLE hnd = get_hnd(epollfd);
    if (hnd == NULL) return -1;

    return ::epoll_wait(hnd, events, maxevents, timeout);
}

int epoll_close(int epollfd) {
    HANDLE hnd = remove_fd(epollfd);
    if (hnd == NULL) return -1;

    return ::epoll_close(hnd);
}

#else
#endif

}
}
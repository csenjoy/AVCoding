#include "SockUtil.h"

#include <mutex>
#include <unordered_map>
#include <string>

#include "util/OnceToken.h"
#include "log/Log.h"
#include "error/uv_errno.h"

namespace avc {
namespace util {

#define CLOSE_FD(fd) do { \
                        if (fd != -1) {\
                            close(fd); \
                            fd = -1; \
                        } \
                    } while(0)

#if defined(_WIN32)
/**
 * 使用OnceToken，在数据段初始化时，调用WSAStartup
*/
static OnceToken g_token(
    []() {
        WORD wVersionRequested = MAKEWORD(2, 2);
        WSADATA wsaData;
        WSAStartup(wVersionRequested, &wsaData);
    }, 
    []() {
        WSACleanup();
    }
);

int ioctl(int fd, long cmd, u_long *ptr) {
    return ioctlsocket(fd, cmd, ptr);
}

int close(int fd) {
    return closesocket(fd);
}
#if (_WIN32_WINNT < _WIN32_WINNT_VISTA)
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size) {
    struct sockaddr_storage ss;
    unsigned long s = size;

    ZeroMemory(&ss, sizeof(ss));
    ss.ss_family = af;

    switch (af) {
    case AF_INET:
        ((struct sockaddr_in *)&ss)->sin_addr = *(struct in_addr *)src;
        break;
    case AF_INET6:
        ((struct sockaddr_in6 *)&ss)->sin6_addr = *(struct in6_addr *)src;
        break;
    default:
        return NULL;
    }
    /* cannot direclty use &size because of strict aliasing rules */
    return (WSAAddressToString((struct sockaddr *)&ss, sizeof(ss), NULL, dst, &s) == 0) ? dst : NULL;
}
int inet_pton(int af, const char *src, void *dst) {
    struct sockaddr_storage ss;
    int size = sizeof(ss);
    char src_copy[INET6_ADDRSTRLEN + 1];

    ZeroMemory(&ss, sizeof(ss));
    /* stupid non-const API */
    strncpy(src_copy, src, INET6_ADDRSTRLEN + 1);
    src_copy[INET6_ADDRSTRLEN] = 0;

    if (WSAStringToAddress(src_copy, af, NULL, (struct sockaddr *)&ss, &size) == 0) {
        switch (af) {
        case AF_INET:
            *(struct in_addr *)dst = ((struct sockaddr_in *)&ss)->sin_addr;
            return 1;
        case AF_INET6:
            *(struct in6_addr *)dst = ((struct sockaddr_in6 *)&ss)->sin6_addr;
            return 1;
        }
    }
    return 0;
}
#endif
#endif // defined(_WIN32)

/**
 * 通过创建ipv6 socket判断是否这次ipv6
*/
static inline bool support_ipv6_l() {
    auto fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        return false;
    }
    //Win32平台没有close关闭socket fd函数。SockUtil提供
    close(fd);
    return true;
}

bool SockUtil::support_ipv6() {
    static auto flag = support_ipv6_l();
    return flag;
}

static int bindSocket4(int fd, uint16_t port, const char *localIp) {
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    //使用inet_pton
    if (-1 == inet_pton(AF_INET, localIp, &(addr.sin_addr))) {
        //ipv4字符串ip转化为4字节网络字节序整型失败, 判断是否是ipv6
        if (strcmp(localIp, "::")) {
            WarnL << "Failed to inet_pton: " << localIp << " error: " << get_uv_errmsg();
        }
        //绑定到任意IP地址
        addr.sin_addr.s_addr = INADDR_ANY;
    }

    //绑定socket
    if (-1 == ::bind(fd, (struct sockaddr *)&addr, sizeof(addr))) {
        WarnL << "Failed to bind socket: " << get_uv_errmsg();
        return -1;
    }
    return 0;
}

static int bindSocket6(int fd, uint16_t port, const char *localIp) {
    struct sockaddr_in6 addrv6;
    bzero(&addrv6, sizeof(addrv6));

    addrv6.sin6_family = AF_INET6;
    addrv6.sin6_port = htons(port);

    if (-1 == inet_pton(AF_INET6, localIp, &(addrv6.sin6_addr))) {
        if (strcmp(localIp, "0.0.0.0")) {
            WarnL << "Failed to inet_pton: " << localIp << " error: " << get_uv_errmsg();
        }
        //绑定到任意IPv6地址
        addrv6.sin6_addr = IN6ADDR_ANY_INIT;
    }
    //绑定socket
    if (-1 == ::bind(fd, (struct sockaddr *)&addrv6, sizeof(addrv6))) {
        WarnL << "Failed to bind socket: " << get_uv_errmsg();
        return -1;
    }
    return 0;
}

static int bindSocket(int fd, uint16_t port, const char *localIp, int family) {
    switch (family) {
        case AF_INET:
            return bindSocket4(fd, port, localIp);
        case AF_INET6:
            return bindSocket6(fd, port, localIp);
    }
    WarnL << "Unkown family";
    return -1;
}

int SockUtil::listen(uint16_t port, const char *localIp, int backLog) {
    int listenFd = -1;

    /**
     * 创建tcp协议socket，根据localIp判断
    */
    int family = support_ipv6() ? (isIpv4(localIp) ? AF_INET : AF_INET6) : AF_INET;
    listenFd = socket(family, SOCK_STREAM, IPPROTO_TCP);
    if (listenFd == -1) {
        WarnL << "Failed to create socket: " << get_uv_errmsg();
        return listenFd;
    }

    //绑定socket
    if (-1 == bindSocket(listenFd, port, localIp, family)) {
        WarnL << "Failed to bind socket: " << get_uv_errmsg();
        CLOSE_FD(listenFd);
        return -1;
    }

    //监听socket
    if (-1 == ::listen(listenFd, backLog)) {
        WarnL << "Failed to listen socket: " << get_uv_errmsg();
        CLOSE_FD(listenFd);
        return -1;
    }

    //监听tcp socket成功，返回对应socket文件描述符
    return listenFd;
}

int SockUtil::connect(const char *host, uint16_t port, bool async, uint16_t localPort, const char *localIp) {
    //获取socket地址
    struct sockaddr_storage addr;

    //优先使用tcp ipv4获取主机域名IP
    if (-1 == getDomainIP(host, port, addr, AF_INET, SOCK_STREAM, IPPROTO_TCP)) {
        WarnL << "Failed to getDomainIP host: " << host; 
        return -1;
    }

    //获取到sockaddr成功，创建socket
    int fd = ::socket(addr.ss_family, SOCK_STREAM, IPPROTO_TCP);
    if (fd == -1) {
        WarnL << "Failed to create socket: " << get_uv_errmsg();
        return -1;
    }

    //socket绑定本地地址
    if (-1 == bindSocket(fd, localPort, localIp, addr.ss_family)) {
        WarnL << "Failed bindSocket" << get_uv_errmsg();
        CLOSE_FD(fd);
        return -1;
    }
    
    //async表示异步，则设置为非阻塞; 否则设置为阻塞
    setNoBlocked(fd, async);

    //阻塞模式下，等待
    socklen_t len = (addr.ss_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
    if (0 == ::connect(fd, (struct sockaddr*)&addr, len)) {
        //connect返回值为0，代表阻塞链接成功
        return fd;
    }
    
    //非阻塞模式，通过错误值EAGAIN，判断异步链接成功
    if (async && UV_EAGAIN == get_uv_error()) {
        return fd;
    }

    //链接失败
    CLOSE_FD(fd);
    return -1;
}

int SockUtil::accept(int fd, sockaddr *addr, socklen_t *addr_len) {
    int peerFd = -1;
    if ((peerFd = ::accept(fd, addr, addr_len)) == -1) {
        WarnL << "Failed to accept" << get_uv_errmsg();
    }
    return peerFd;
}

int SockUtil::setNoDelay(int fd, bool on) {
    return -1;
}

int SockUtil::setNoBlocked(int fd, bool on){
#if defined(WIN32)
    unsigned long ul = on;
#else
    int ul = on;
#endif
    int ret = ioctl(fd, FIONBIO, &ul);
    if (ret == -1) {
        TraceL << "Failed to ioctl FIONBIO: " << get_uv_errmsg();
    }
    return ret;
}

int SockUtil::setCloOnExec(int fd, bool on)
{
    return 0;
}

bool SockUtil::isIpv4(const char *ip) {
    //通过inet_pton判断ip是否为ipv4
    in_addr addr;
    return 1 == inet_pton(AF_INET, ip, &addr);
}

using getsockname_type = decltype(getsockname);
static bool get_sock_addr(int fd, struct sockaddr_storage &addr, getsockname_type func) {
    socklen_t addr_len = sizeof(addr);
    if (-1 == func(fd, (struct sockaddr *)&addr, &addr_len)){
        return false;
    }
    return true;
}

static uint16_t get_socket_port(int fd, getsockname_type func) {
    struct sockaddr_storage addr;
    if (!get_sock_addr(fd, addr, func)) {
        //获取socket对应的sockaddr_storage失败
        return 0;
    }
    return SockUtil::inet_port((struct sockaddr *)&addr);
}

uint16_t SockUtil::get_local_port(int fd)
{
    return get_socket_port(fd, getsockname);
}

uint16_t SockUtil::get_peer_port(int fd) {
    return get_socket_port(fd, getpeername);
}

struct sockaddr_storage SockUtil::makeSockAddr(const char *ip, uint16_t port) {
    struct sockaddr_storage addrStorage;
    bzero(&addrStorage, sizeof(addrStorage));

    struct sockaddr_in* addrv4 = (struct sockaddr_in *)&addrStorage;
    if (1 == inet_pton(AF_INET, ip, &(addrv4->sin_addr))) {
        //字符串ipv4转为网络字节序成功
        addrv4->sin_family = AF_INET;
        addrv4->sin_port = htons(port);
        return addrStorage;
    }
    struct sockaddr_in6* addrv6 = (struct sockaddr_in6*)&addrStorage;
    if (1 == inet_pton(AF_INET6, ip, &(addrv6->sin6_addr))) {
        //字符串ipv6转为网络字节序成功
        addrv6->sin6_family = AF_INET6;
        addrv6->sin6_port = htons(port);
        return addrStorage;
    }

    //转换失败，抛出异常
    throw std::runtime_error(StrPrinter << "Failed to makeSockAddr: " << ip);
}

uint16_t SockUtil::inet_port(struct sockaddr *sockaddr)
{
    switch (sockaddr->sa_family) {
        case AF_INET:
            return ntohs(((struct sockaddr_in*)sockaddr)->sin_port);
            break;
        case AF_INET6:
            return ntohs(((struct sockaddr_in6*)sockaddr)->sin6_port);
            break;
     }
    return 0;
}

class DnsCache {
public:
    static DnsCache &instance() {
        static DnsCache instance;
        return instance;
    }

    bool getDomainIP(const char *host, sockaddr_storage &storage, int ai_family = AF_INET,
                     int ai_socktype = SOCK_STREAM, int ai_protocol = IPPROTO_TCP, int expire_sec = 60) {
        try {
            storage = SockUtil::makeSockAddr(host, 0);
            return true;
        } catch (...) {
            auto item = getCacheDomainIP(host, expire_sec);
            if (!item) {
                item = getSystemDomainIP(host);
                if (item) {
                    setCacheDomainIP(host, item);
                }
            }
            if (item) {
                auto addr = getPerferredAddress(item.get(), ai_family, ai_socktype, ai_protocol);
                memcpy(&storage, addr->ai_addr, addr->ai_addrlen);
            }
            return (bool)item;
        }
    }

private:
    class DnsItem {
    public:
        std::shared_ptr<struct addrinfo> addr_info;
        time_t create_time;
    };

    std::shared_ptr<struct addrinfo> getCacheDomainIP(const char *host, int expireSec) {
        std::lock_guard<std::mutex> lck(_mtx);
        auto it = _dns_cache.find(host);
        if (it == _dns_cache.end()) {
            //没有记录
            return nullptr;
        }
        if (it->second.create_time + expireSec < time(nullptr)) {
            //已过期
            _dns_cache.erase(it);
            return nullptr;
        }
        return it->second.addr_info;
    }

    void setCacheDomainIP(const char *host, std::shared_ptr<struct addrinfo> addr) {
        std::lock_guard<std::mutex> lck(_mtx);
        DnsItem item;
        item.addr_info = std::move(addr);
        item.create_time = time(nullptr);
        _dns_cache[host] = std::move(item);
    }

    std::shared_ptr<struct addrinfo> getSystemDomainIP(const char *host) {
        struct addrinfo *answer = nullptr;
        //阻塞式dns解析，可能被打断
        int ret = -1;
        do {
            ret = getaddrinfo(host, nullptr, nullptr, &answer);
        } while (ret == -1 && get_uv_error(true) == UV_EINTR);

        if (!answer) {
            WarnL << "getaddrinfo failed: " << host;
            return nullptr;
        }
        return std::shared_ptr<struct addrinfo>(answer, freeaddrinfo);
    }

    struct addrinfo *getPerferredAddress(struct addrinfo *answer, int ai_family, int ai_socktype, int ai_protocol) {
        auto ptr = answer;
        while (ptr) {
            if (ptr->ai_family == ai_family && ptr->ai_socktype == ai_socktype && ptr->ai_protocol == ai_protocol) {
                return ptr;
            }
            ptr = ptr->ai_next;
        }
        return answer;
    }

private:
    std::mutex _mtx;
    std::unordered_map<std::string, DnsItem> _dns_cache;
};//class DnsCache


bool SockUtil::getDomainIP(const char *host, uint16_t port, sockaddr_storage &addr, 
                           int family, int type, int protocol, 
                           int dnsCacheExpiredSeconds) {
    auto flag = DnsCache::instance().getDomainIP(host, addr, family, type, protocol, dnsCacheExpiredSeconds);
    if (flag) {
        switch (addr.ss_family)
        {
        case AF_INET:
            ((struct sockaddr_in*)&addr)->sin_port = htons(port);
            break;
        case AF_INET6:
            ((struct sockaddr_in6*)&addr)->sin6_port = htons(port);
        default:
            break;
        }
    }
    return false;
}

}
}

#ifndef NETWORK_SOCKUTIL_H
#define NETWORK_SOCKUTIL_H

#include <stdint.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment (lib, "Ws2_32.lib")
#pragma comment(lib,"Iphlpapi.lib")
#else
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif // defined(_WIN32)

namespace avc {
namespace util {

#if defined(_WIN32)
#ifndef socklen_t
#define socklen_t int
#endif //!socklen_t
int ioctl(int fd, long cmd, u_long *ptr);
int close(int fd);
#endif // defined(_WIN32)

struct SockUtil {

static bool support_ipv6();

/**
 * 创建一个监听tcp socket
 * @port 监听端口 port为0时，使用随机端口
 * @localIp 监听IP
 * @backLog 未accept的套接字数量
*/
static int listen(uint16_t port, const char* localIp, int backLog = 1024);
/**
 * @param host服务器ip或域名
*/
static int connect(const char *host, uint16_t port, bool async = true, uint16_t localPort = 0, const char* localIp = "::");
static int accept(int fd, struct sockaddr *addr, socklen_t *addr_len);
static int setNoDelay(int fd, bool on = true);
static int setNoBlocked(int fd, bool on = true);
/**
 * UNIX系统中子进程继承父进程的文件描述符，
 *      当文件描述符设置成Close-on-exec时，子进程会关闭对应文件描述符
*/
static int setCloOnExec(int fd, bool on = true);

static bool isIpv4(const char *ip);

static uint16_t get_local_port(int fd);
static uint16_t get_peer_port(int fd);

/**
 * 处理sockaddr相关
 * @note 如果转为ip处理失败，则抛出运行时异常
*/
static struct sockaddr_storage makeSockAddr(const char *ip, uint16_t port);
static uint16_t inet_port(struct sockaddr *sockaddr);

static bool getDomainIP(const char *host, uint16_t port, struct sockaddr_storage &addr, 
                        int family = AF_INET, int type = SOCK_STREAM, int protocol = IPPROTO_TCP,
                        int dnsCacheExpiredSeconds = 60);

};//struct SockUtil

}
}

#endif
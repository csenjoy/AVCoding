#ifndef NETWORK_SOCKET_H
#define NETWORK_SOCKET_H

#include <memory>
#include <list>

#include "util/Util.h"

#include "network/Buffer.h"
#include "network/BufferSock.h"
#include "network/SockUtil.h"
#include "poller/EventPollerPool.h"
#include "util/MutexWrapper.h"

namespace avc {
namespace util {

class SockNum {
public:
    enum Type {
        kTypeInvalid,
        kTypeTcp,//tcp建立连接的socket包含：1）主动发起连接的socket 2）accept产生连接的socket
        kTypeUdp,//udp socket不区分，客户端和服务端
        kTypeTcpServer //tcp socket需要区分是服务端的acceptor；或者建立连接的socket
    };//enum 

    using Ptr = std::shared_ptr<SockNum>;

    AVC_STATIC_CREATOR(SockNum)

    ~SockNum() {
#if defined(WIN32)
        ::shutdown(fd_, SD_BOTH);
#else
        ::shutdown(fd_, SHUT_RDWR);
#endif
        close(fd_);
        fd_ = -1;
        type_ = Type::kTypeInvalid;
    }

    int rawFD() const { return fd_; }
    int type() const { return type_; }
private:
    SockNum() {}
    SockNum(int fd, Type type) : fd_(fd), type_(type) {}
private:
    int fd_ = -1;
    Type type_ = Type::kTypeInvalid;
};//class SockNum

class SockFD {
public:
    using Ptr = std::shared_ptr<SockFD>;
    ~SockFD() {
        detachEvent();
    }

    AVC_STATIC_CREATOR(SockFD)

    SockNum::Ptr getSockNum() const { return sock_num_; }

    int rawFd() const {
        if (sock_num_) return sock_num_->rawFD();
        return -1;
    }

    int type() const {
        if (sock_num_) return sock_num_->type();
        return SockNum::kTypeInvalid;
    }
private:
    SockFD(int fd, SockNum::Type type, EventPoller::Ptr poller) 
        : sock_num_(SockNum::create(fd, type)), poller_(poller) {}
    SockFD(const SockFD &sockFd);
private:
    int detachEvent() {
        if (sock_num_ && poller_) {
            return poller_->detachEvent(sock_num_->rawFD());
        }
        return -1;
    }
private:
    SockNum::Ptr sock_num_;
    EventPoller::Ptr poller_;
};//class SockFD

class SocketException : public std::exception {
public:
    SocketException(int err, std::string &&);
};//class SocketException

/**
 * 异步Socket接口
 *      Socket职责：
 *          1）封装socket文件描述符
 *          2）创建不同类型的socket
 *          3) 
 * 
*/
class Socket : public std::enable_shared_from_this<Socket> {
public:
    enum {
        kSockTypeTcp,
        kSockTypeUdp,
    };
    using Ptr = std::shared_ptr<Socket>;
    using OnRead = std::function<void(Buffer::Ptr, struct sockaddr *addr, socklen_t len)>;

    AVC_STATIC_CREATOR(Socket)
    ~Socket();

    void setOnRead(OnRead &&cb);
    /**
     * 创建UDP Socket
    */
    int bindUdpSocket(uint16_t port, const std::string &ip = "::", bool reuseAddr = true);

    /**
     * send族函数，处理发送各种类型的buffer，数据最后统一封装成Buffer::Ptr 
     * @tryFlush  Buffer::Ptr会被放在缓冲区，调用tryFlush会立即写入socket
    */
    int send(std::string &&bufer, struct sockaddr *addr = nullptr, socklen_t len = 0, bool tryFlush = true);
    int send(const char *buffer, size_t size = 0, struct sockaddr *addr = nullptr, socklen_t len = 0, bool tryFlush = true);
    int send(Buffer::Ptr buffer, struct sockaddr *addr = nullptr, socklen_t len = 0, bool tryFlush = true);
private:
    /**
     * 将socket文件描述符，封装成Socket类处理
     * 用户需要提供文件描述符fd以及对应socket类型
    */
    int fromSockFd(int fd, SockNum::Type type);
    /**
     * 克隆另外一个Socket对象
     *     例如，创建一个新的Socket与other属于不同的EventPoller。通过拷贝other后，
     *          可以使同一个SockFd被不同的EventPoller处理。
     *     主要用于：1）Tcp Server端的Acceptor Socket。使得同一个Acceptor能够在多个EventPoller中listen
     *                 从而增加并发性
     *              
     *      
    */
    int cloneSocket(const Socket &other);

    /**
     * @param isBufferSock BufferSock对象；否则为Buffer对象
     *                     当Socket为UDP类型是，为BufferSock对象，携带struct sockaddr_storage信息
    */
    int send_l(Buffer::Ptr buffer, bool isBufferSock, bool tryFlush = true);

    /**
     * 注意flush数据时，分为：调用线程触发flush, 例如调用send函数并指定tryFlush参数，导致flush
     *                      EventPoller线程触发flush， socket可写导致flush
    */                      
    int flushAll();

    int flushData(int fd, int type, bool isEventPollerThread);

    /**
     * 关于可写事件
     *      1）什么时候需要可写事件？ 
     *           i) Socket中一级和二级缓存中，存在数据时，需要可写事件
     *           ii) flushData发送数据时，出现部分发送且出错原因时UV_EAGIN时，需要可写事件
     *      2) 什么时候移除可写事件？
     *          i）Socket中一级和二级缓存中，没有数据时，移除可写事件
     *          ii) 可写事件触发flushData，导致Socket一级和二级缓存，数据发送完毕时，移除可写事件
     *                  此时也会触发onFlushed
     *      3) 什么时候触发可写事件
     *          i）建立连接后，会发生可写事件
     *          ii）socket写缓冲区空闲（频繁发生）
     * 注意： 写事件回调会频繁的发生，因此需要在正确的时候使用写事件
     *          1）出现部分写成功，并且出错原因时UV_EAGIN时
     *          2) Socket在建立连接之前，已经往Socket缓冲区写入数据
     *              例如，Socket创建SockFD之前，已经调用send函数，并且指定tryFlush为false时，会出现此种情况。
     *                    这个时候需要可写事件，触发flushData，从而将缓冲数据写入socket
     *       
    */
    void startWritableEvent(int fd);
    void stopWritableEvent(int fd);
private:
    explicit Socket(EventPoller::Ptr poller = nullptr);
    //explicit Socket(int fd, int type, EventPoller::Ptr poller = nullptr);
    //explicit Socket(const Socket& rhs, EventPoller::Ptr poller = nullptr);
    
    int attachEvent(int fd, int type);
    void setSocketFD(SockFD::Ptr sockFd);
    void closeSocket();

    void onAcceptable();
    /**
     * 收到socket的读事件
     * 如何接收数据？
     *      1）Socket属于某个EventPoller，读事件发生时，串行触发onReadable
     *         因此接收数据时，可以使用同一个Buffer接收数据
     *      2) 循环调用recvfrom，知道EOF或者EAGIN
    */
    void onReadable(int fd, int type);
    void onWritable(int fd, int type);
    //void emitError();
    void emitError(const SocketException& exception) noexcept;

    /**
     * Socket的写缓冲buffer清空时（通常是一级缓存为空） 
     *        并且出现部分发送且出错原因时UV_EAGIN时，才会触发onFlushed回调
     * 正常发送（发送数据量没有超过socket写缓冲区时），并不会导致onFlushed回调
    */
    void onFlushed();
private:
    EventPoller::Ptr poller_;

    OnRead on_read_;
    MutexWrapper<std::recursive_mutex> mtx_event_;
    /**
     * 使用recursive_mutex而不是mutex
     * 
    */
    MutexWrapper<std::recursive_mutex> mtx_fd_;
    SockFD::Ptr sock_fd_;

    /**
     * Udp发送目标地址
    */
    std::shared_ptr<struct sockaddr_storage> udp_send_dst_;

    /**
     * 接收数据，用来判断是否注册读事件
    */
    std::atomic<bool> enable_recv_{true};

    /**
     * socket是否可写, 例如socket写缓冲满了就不可写了
     * 此标志用于控制用户是否需要flush， 当存在写事件时，不需要用户调用flush
     * 
     *     注册写事件后，sendable_不可写
     * 
     *     默认情况注册了写事件，因此此时sendable_应该为false
     *     (避免写事件回调，和用户调用send，同时触发flushData)
     * 
     *      stopWritableEvent时，设置sendable_为true
     *      startWritableEvent时，设置sendable_为false    
     * */
    std::atomic<bool> sendable_{false};
    /**
     * 发送buffer缓存, 用户想要发送的Buffer，首先被放置在这个缓存中，这个缓存成为一级缓存
     * 注意：如果是udp的时，Buffer会被封装成BufferSock，然后放入此缓存中
     * 
    */
    MutexWrapper<std::recursive_mutex> mtx_send_buffer_waiting_;
    /**
     * bool参数描述这个节点是BufferSock；否则为Buffer
    */
    std::list<std::pair<Buffer::Ptr, bool>> send_buffer_waiting_;
    /**
     *  此缓存为二级缓冲， 发送时将一级缓存数据移动到二级缓存
     *  为什么需要二级缓存？
     *      发送失败时（例如窗口大小为0导致发送失败），需要将数据回滚到二级缓存中
     *      等待可写后继续发送
     * 
     * BufferList
     *  BufferList合并多个Buffer发送，同时通过BufferList实现的send接口
     *  1）屏蔽了udp与tcp发送接口不一致问题（例如send与sendto）
     *  2）提供多种写实现，例如iovec写
    */
    MutexWrapper<std::recursive_mutex> mtx_send_buffer_sending_;
    std::list<BufferList::Ptr> send_buffer_sending_;
    //std::list<>
};//class Socket


}
}

#endif
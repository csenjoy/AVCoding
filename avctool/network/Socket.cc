#include "Socket.h"

#include "network/SockUtil.h"
#include "error/uv_errno.h"

namespace avc {
namespace util {

Socket::~Socket() {
    closeSocket();
}

void Socket::setOnRead(OnRead &&cb) {
    if (cb == nullptr) {
        cb = [this](Buffer::Ptr buffer, struct sockaddr *, socklen_t)->void {};
    }

    LOCK_GUARD(mtx_fd_);
    on_read_ = std::move(cb);
}

int Socket::bindUdpSocket(uint16_t port, const std::string &ip, bool reuseAddr) {
    //创建udp socket文件描述符
    int fd = SockUtil::bindUdpSocket(port, ip.c_str(), reuseAddr);
    if (fd == -1) {
        WarnL << "Failed to bindUdpSocket " << ip << ":" << port;
        return -1;
    }

    return fromSockFd(fd, SockNum::kTypeUdp);
}

int Socket::send(std::string&& buffer, sockaddr* addr, socklen_t len, bool tryFlush) {
    auto bufferString = BufferString::create(std::move(buffer));
    return send(std::move(bufferString), addr, len, tryFlush);
}

int Socket::send(const char* buffer, size_t size, sockaddr* addr, socklen_t len, bool tryFlush) {
    auto bufferRow = BufferRaw::create(buffer, size);
    return send(std::move(bufferRow), addr, len, tryFlush);
}

int Socket::send(Buffer::Ptr buffer, struct sockaddr *addr, socklen_t len, bool tryFlush) {
    //addr为nullptr时
    if (!addr) {
        if (!udp_send_dst_) {
            //没有设置udp目标地址，也没有指定发送地址
            //buffer为Buffer类型对象
            return send_l(std::move(buffer), false, tryFlush);
        }

        //设置有udp发生目标地址时，udp发送可以不指定目标地址
        addr = (struct sockaddr*)(udp_send_dst_.get());
        len = SockUtil::get_sockaddr_len(addr);
    }
    //buffer为B
    return send_l(BufferSock::create(std::move(buffer), addr, len), true, tryFlush);
}

int Socket::send_l(Buffer::Ptr buffer, bool isBufferSock, bool tryFlush) {
    //判断buffer大小
    auto size = buffer ? buffer->size() : 0;
    if (!size) {
        return 0;
    }

    {
        LOCK_GUARD(mtx_send_buffer_waiting_);
        send_buffer_waiting_.emplace_back(std::make_pair(buffer, isBufferSock));
    }

    /**
     * 指定tryFlush，则立即调用刷新函数，发送缓存数据
    */
    if (tryFlush) {
        if (-1 == flushAll()) {
            //发送失败，返回-1
            return -1;
        }
    }
    return size;
}

int Socket::flushAll() {
    //发送逻辑访问fd，因此需要加锁
    LOCK_GUARD(mtx_fd_);

    if (sock_fd_ == nullptr) {
        //断开连接后者发送超时
        WarnL << "sock_fd_ is nullptr";
        return -1;
    }

    //socket可写，则调用flushData写数据到socket
    if (sendable_) {
        return flushData(sock_fd_->rawFd(), sock_fd_->type(), false);
    }

    //socket不可写时，判断是否存在超时
    return -1;
#if 0
    decltype(send_buffer_waiting_) send_buffer_waiting_tmp;
    {
        //获取当前待发送buffer
        LOCK_GUARD(mtx_send_buffer_waiting_);
        send_buffer_waiting_tmp.swap(send_buffer_waiting_);
    }

    //

    std::list<BufferList::Ptr> send_buffer_sending_tmp;
    if (!send_buffer_waiting_tmp.empty()) {
        send_buffer_sending_tmp.push_back(BufferList::create(std::move(send_buffer_waiting_tmp), nullptr, sock_fd_->type() == SockNum::kTypeUdp));
    }

    while (!send_buffer_sending_tmp.empty()) {
        auto packets = send_buffer_sending_tmp.front();
        int n = packets->send(sock_fd_->rawFd());
        //全部或部分发送成功
        if (n > 0) {
            if (packets->empty()) {
                //发从完成
                send_buffer_sending_tmp.pop_front();
            }

            //没有发送完成, 跳出发送循环
            //并且回滚数据
            break;
        }

        //发送失败（一个都没有发送成功)
        break;

    }
    return 0;
#endif
}


int Socket::flushData(int fd, int type, bool isEventPollerThread) {
    //首先处理发送二级缓存中的BufferList
    decltype(send_buffer_sending_) send_buffer_sending_tmp;
    {
        LOCK_GUARD(mtx_send_buffer_sending_);
        send_buffer_sending_.swap(send_buffer_sending_tmp);
    }

    //二级缓存中没有数据，则处理一级缓存中的数据
    if (send_buffer_sending_tmp.empty()) {
        /**
         * 一级缓存中也没有数据，则等待send函数调用
         * 此时可以移除socket的写事件，避免写事件回调(因为下一次send函数调用，会触发flush)
        */
        LOCK_GUARD(mtx_send_buffer_waiting_);
        if (send_buffer_waiting_.empty()) {

            if (isEventPollerThread) {
                /**
                 * isEventPollerThread条件下，才调用stopWriteableEvent的原因
                 *      （1）如果用户调用的flushData函数，没有必要移除写事件,
                 *           因此如果存在写事件的话，下次触发写事件的时候，就会移除写事件
                */
                stopWritableEvent();
                /**
                 * 
                */
                onFlushed();
            }
            return 0;
        }

        //一级缓存中存在数据，则创建BufferList发送数据
        send_buffer_sending_tmp.emplace_back(BufferList::create(std::move(send_buffer_waiting_), nullptr, type == SockNum::kTypeUdp));
    }

    //发送数据
    while(!send_buffer_sending_tmp.empty()) {
        auto packet = send_buffer_sending_tmp.front();
        int n = packet->send(fd);

        //发送成功
        if (n > 0) {
            if (packet->empty()) {
                //这个BufferList完全发送成功, 移除这个BufferList后，则继续发送
                send_buffer_sending_tmp.pop_front();
                continue;
            }

            //部分发送成功，添加可写事件回调（socket写就绪时，触发调用flushData）
            if (!isEventPollerThread) {
                /**
                 * 不是EventPoller线程触发时，添加可写事件
                 * 如果是EventPoller线程触发，说明已经存在可写事件，并不需要添加
                */

                startWritableEvent();
            }
            break;
        }

        //发送失败，判断具体出错类型
        if (UV_EAGAIN == get_uv_error()) {
            //异步socket返回重试，则添加可写事件（socket写就绪时，触发调用flushData)
            if (!isEventPollerThread) {
                /**
                 * 不是EventPoller线程触发时，添加可写事件
                 * 如果是EventPoller线程触发，说明已经存在可写事件，并不需要添加
                */
                startWritableEvent();
                break;
            }
        }
        //其他错误类型，说明socket出现异常
        //emitError();
        return -1;;
    }

    if (!send_buffer_sending_tmp.empty()) {
        //未发送完成数据，添加到二级缓存
        LOCK_GUARD(mtx_send_buffer_sending_);
        send_buffer_sending_.swap(send_buffer_sending_tmp);
        /**
         * 二级缓存数据，没有发送完成。直接返回成功
         *      运行到这个地方，说明: 1）socket没有出错（emitError）
         *                          2）并且已经注册好写事件回调，等待socket可写后，再次触发flushData
        */  
        return 0;
    }

    /**
     * 二级缓存数据，全部发送完成
     * 此处需要判断，是谁触发的flushData
     *      如果是EventPoller触发，则继续调用flushData，原因是，此时一级缓存可能有新来的数据（用户调用send函数，没有tryFlush）
     *      如果是send函数触发，则直接返回成功         
    */
   return isEventPollerThread ? flushData(fd, type, isEventPollerThread) : 0;
}

int Socket::fromSockFd(int fd, SockNum::Type type) {
    closeSocket();
    auto sockFd = SockFD::create(fd, type, poller_);
    SockUtil::setCloOnExec(fd);
    SockUtil::setNoBlocked(fd);

    setSocketFD(sockFd);
    //注册网络I/O事件
    if (-1 == attachEvent(fd, type)) {
        WarnL << "Failed to attach fd event";
        return -1;
    }
 
    return 0;
}

Socket::Socket(EventPoller::Ptr poller) {
    if (poller == nullptr) {
        poller = EventPollerPool::instance().getEventPoller();
    }
    poller_ = poller;
}

int Socket::attachEvent(int fd, int type) {
    if (sock_fd_ == nullptr || sock_fd_->getSockNum() == nullptr) {
        WarnL << "sock_fd_ is nullptr";
        return -1;
    }


    int ret = -1;
    std::weak_ptr<Socket> self = shared_from_this();
    if (type == SockNum::kTypeTcpServer) {
        //Tcp Acceptor监听读事件与异常事件即可
        ret = poller_->attachEvent(fd,
                EventPoller::Event::kEventRead | EventPoller::Event::kEventError,
                [this, fd, type, self](int events)->void {
                    auto socket = self.lock();
                    if (socket == nullptr) return;

                    if (events & EventPoller::kEventRead) {
                        socket->onAcceptable();
                    }
                    if (events & EventPoller::kEventError) {

                    }
                }
              );
    }
    else {
        /**
         * 回调I/O事件时，对应的fd与当前socket的fd不一致会发生吗？
         *      例如，将一个调用Socket::cloneSocket函数
        */
        ret = poller_->attachEvent(fd,
                EventPoller::Event::kEventRead | EventPoller::Event::kEventWrite | EventPoller::Event::kEventError,
                [this, fd, type, self](int events)->void {
                    auto socket = self.lock();
                    //socket被销毁（用户持有的socket被销毁）
                    if (socket == nullptr) { return; }

                    if (events & EventPoller::Event::kEventRead) {
                        socket->onReadable(fd, type);
                    }
                    if (events & EventPoller::Event::kEventWrite) {
                        socket->onWritable(fd, type);
                    }
                    if (events & EventPoller::Event::kEventError) {
                    }
                }
        );
    }
    return ret;
}

void Socket::setSocketFD(SockFD::Ptr sockFd) {
    LOCK_GUARD(mtx_fd_);
    sock_fd_ = sockFd;
}

void Socket::closeSocket() {
    {
        LOCK_GUARD(mtx_fd_);
        sock_fd_ = nullptr;
    }
}

void Socket::onAcceptable() {

}

void Socket::onReadable(int fd, int type) {

}

void Socket::onWritable(int fd, int type) {

}

void Socket::emitError(const SocketException& exception) noexcept {

}

}
}
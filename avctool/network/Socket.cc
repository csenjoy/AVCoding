#include "Socket.h"

#include <assert.h>

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

    LOCK_GUARD(mtx_event_);
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
                stopWritableEvent(fd);
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
                startWritableEvent(fd);
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
                startWritableEvent(fd);
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
        /**
         * 注意：
         *      1）send_buffer_sending_中可能存在数据
         *          因为flushData存在多线程访问情况，当其中一个线程调用flushData，
         *          将一级缓存转为二级缓存并且发送失败时会产生数据
         *      2）flushData存在多线程访问情况，可能会导致数据发送乱序？
         *          由于每次发送时，都是加锁获取有序列表，然后在发送，此时发送时有序的，但是遇到其他线程调用flushData时，
         *          可能导致两个有序发送队列，变成无序。
         * 
         *          并不会出现多线程flushData情况：
         *              EventPoller触发的前提是，注册了写事件。
         *                  1）初始化时，默认注册了写事件，此时sendable_为true
         *                  2）缓存清空后，取消写事件,此时sendable_为true
         *                  3）发送失败，重新注册写事件，此时sendable_为flase
         *              关于情况1）：
         *                  初始化bindUdpSocket之前，调用send添加一级缓存，此时既有写事件，sendable_又是true（默认情况下）
         *                  这种情况可能存在多线程访问flushData。
         *                  但是zltoolkit，bindUdpSocket时，会情况之前添加的一级缓存，因此
         *                 
         *                      
         *      3）将未发送成功的数据放入send_buffer_sending_中时，需要保证顺序吗？如果需要，如何实现？
         *          ZLToolKit的实现： 将tmp数据放在队列前面:
         *              send_buffer_sending_.swap(send_buffer_sending_tmp);
         *              send_buffer_sending_.append(send_buffer_sending_tmp);
         *      
        */

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


void Socket::startWritableEvent(int fd) {
    //注册写事件时，禁用sendable_（禁止用户flushData操作）
    sendable_ = false;
    
    /**
     * 访问sock_fd了 是否需要加锁
     *      避免加锁，fd传递参数进来
    */
    //LOCK_GUARD(mtx_fd_);
    //注册写事件
    int flag = enable_recv_ ? EventPoller::Event::kEventRead : 0;
    poller_->modifyEvent(sock_fd_->rawFd(), flag | EventPoller::Event::kEventWrite | EventPoller::Event::kEventError);
}
void Socket::stopWritableEvent(int fd) {
    /**
     * 取消读事件，启用sendable_(用户调用flushData，触发写socket)
    */
    sendable_ = true;

    //LOCK_GUARD(mtx_fd_);
    int flag = enable_recv_ ? EventPoller::Event::kEventRead : 0;
    poller_->modifyEvent(sock_fd_->rawFd(), flag | EventPoller::Event::kEventError);
}

Socket::Socket(EventPoller::Ptr poller) {
    if (poller == nullptr) {
        poller = EventPollerPool::instance().getEventPoller();
    }
    poller_ = poller;
}

int Socket::attachEvent(int fd, int type) {
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
    //默认情况sendable_为false，因为会注册写事事件
    sendable_ = false;
    enable_recv_ = true;

    {
        LOCK_GUARD(mtx_fd_);
        sock_fd_ = nullptr;
    }
}

void Socket::onAcceptable() {

}

void Socket::onReadable(int fd, int type) {
    /**
     * Socket收到可读事件
    */
   //assert();
   
    /**
     * 因为读事件是在EventPoller线程中串行发生的，所以对于同一个EventPoller
     * 下的所有Socket，都可以使用同一个Buffer来读取数据，并回调给上层
     * 注意：上层用户处理时，如果异步处理，则需要拷贝Buffer(因为buffer需要给下一个Socket读使用)
    */
    int nread = 0;
    auto buffer = poller_->getSharedBuffer();

    struct sockaddr_storage addr; socklen_t len;
    while(enable_recv_) {
        /**
         * 异步Socket接受数据时，出错原因时UV_INTR时，
         * 需要重新调用接口接收数据
        */
        do {
            nread = ::recvfrom(fd, buffer->data(), buffer->size() - 1, 0, (struct sockaddr *)&addr, &len);
        } while (-1 == nread && UV_EINTR == get_uv_error());

        if (nread == 0) {
            if (type == SockNum::kTypeTcp) {
                //tcp连接读到eof，需要处理出错
                //emitError()
            }
            else {
                //udp socket时，打印错误即可，不需要抛出异常
                WarnL << "Recv eof on udp socket";
            }
            //eof error
            return;
        }

        if (nread == -1) {
            int err = get_uv_error();
            if (UV_EAGAIN == err) {
                //异步I/o数据未准备好, 直接返回
                return;
            }

            //其他类型出错
            if (SockNum::kTypeTcp == type) {
                //tcp触发异常
                //emitError();
            }
            else {
                WarnL << "Recv err on udp socket: " << get_uv_errmsg();
            }
            return;
        }

        //接收成功
        *(buffer->data() + nread) = '\0';
        buffer->setSize(nread);

        LOCK_GUARD(mtx_event_);
        try {
            on_read_(buffer, (struct sockaddr *)&addr, len);
        }
        catch(std::exception &e) {
            WarnL << "Exception occurred when emit on_read " << e.what();
        }
        //继续接收
    }
   
}

void Socket::onWritable(int fd, int type) {

}

void Socket::emitError(const SocketException& exception) noexcept {

}

void Socket::onFlushed() {

}

}
}
#ifndef NETWORK_BUFFERSOCK_H
#define NETWORK_BUFFERSOCK_H

#include <list>

#include "network/SockUtil.h"
#include "network/Buffer.h"
#include "util/Util.h"

namespace avc {
namespace util {

/**
 * Buffer with struct sockaddr
*/
class BufferSock : public Buffer {
public:
    using Ptr = std::shared_ptr<BufferSock>;

    AVC_STATIC_CREATOR(BufferSock)

    ~BufferSock() {}
private:
    BufferSock(Buffer::Ptr &&buffer, struct sockaddr *addr = nullptr, socklen_t len = 0);
public:
    char* data() const override {
        return buffer_->data();
    }
    size_t size() const override {
        return buffer_->size();
    }

    const struct sockaddr* sockaddr() const {
        return (const struct sockaddr*)(&addr_);
    }

    socklen_t socklen() const {
        return addr_len_;
    }
private:
    struct sockaddr_storage addr_;
    socklen_t addr_len_;
    Buffer::Ptr buffer_;
};//class BufferSock

/**
 * BufferList接口，用于多个buffer一起发送
 *  例如iovec接口和
*/
class BufferList {
public:
    using Ptr = std::shared_ptr<BufferList>;
    using SendResult = std::function<void(const Buffer::Ptr&, bool)>;
    //AVC_STATIC_CREATOR(BufferList);
    virtual ~BufferList() {}

    static BufferList::Ptr create(std::list<std::pair<Buffer::Ptr,bool>>&& data, 
                                  SendResult sendResult, bool isUdp);

    virtual int count() const = 0;
    virtual bool empty() const = 0;
    virtual int send(int fd, int flags = 0) = 0;
};//class BufferList

}
}

#endif
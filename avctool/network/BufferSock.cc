#include "BufferSock.h"

#include "util/Util.h"
#include "log/Log.h"
#include "error/uv_errno.h"

namespace avc {
namespace util {

BufferSock::BufferSock(Buffer::Ptr&& buffer, struct sockaddr* addr, socklen_t len) 
    : buffer_(std::move(buffer)), addr_len_(len) {
    bzero(&addr_, sizeof(addr_));
    memcpy(&addr_, addr, len);
}
/// <summary>
/// @todo 成功发送后，需要回调通知上层用户，用于统计流量
///       class BufferCallback;
/// </summary>
class BufferSendTo : public BufferList {
public:
    AVC_STATIC_CREATOR(BufferSendTo)

    ~BufferSendTo() {}

    int count() const override {
        return data_.size();
    }

    bool empty() const override {
        return data_.empty();
    }

    int send(int fd, int flags) override {
        int sent = 0; 
        ssize_t n;
        while (!data_.empty()) {
            auto buffer = data_.front().first;
            if (is_udp_) {
                auto bufferSock = getBufferSock(data_.front());
                n = ::sendto(fd, 
                             buffer->data() + offset_, buffer->size() - offset_, flags,
                             bufferSock ? bufferSock->sockaddr() : nullptr,
                             bufferSock ? bufferSock->socklen() : 0);
            }//if (is_udp_)
            else {
                //tcp发送
                n = ::send(fd, buffer->data() + offset_, buffer->size() - offset_, flags);
            }

            /**
             * 需要考虑发送失败和部分发送成功情况
            */
            if (n <= 0) {
                //发送失败
                WarnL << "sendto failed: " << get_uv_errmsg();
                
                if (UV_EAGAIN == get_uv_error()) {
                    //发送错误类型是重试，则继续发送
                    continue;
                }
                //其他出错原因，则返回
                break;
            }
            
            //成功发送n个字节
            offset_ += n;
            if (offset_ == buffer->size()) {
                //发送成功，移除已发送Buffer
                data_.pop_front();
                //发送成功后，重新设置当前发送offset
                offset_ = 0;
            }

            //更新已发送字节数
            sent += n;
            //部分发送或则完全发送成功, 继续发送
            continue;
        }//while (!data_.empty()) 

        return sent ? sent : -1;
    }

    static BufferSock::Ptr getBufferSock(const std::pair<Buffer::Ptr, bool> &pair) {
        //不是BufferSock类型，并不能转化为BufferSock对象
        if (!pair.second) {
            return nullptr;
        }
        return std::static_pointer_cast<BufferSock>(pair.first);
    }
private:
    BufferSendTo(std::list<std::pair<Buffer::Ptr, bool>>&& data, SendResult sendResult, bool isUdp)
        : data_(std::move(data)), is_udp_(isUdp) {
    }
private:
    bool is_udp_;
    /**
     * 记录当前增在发送的Buffer的偏移，用于处理部分发送成功情况
    */
    size_t offset_ = 0;
    std::list<std::pair<Buffer::Ptr, bool>> data_;
};//class BufferSendTo
/////////////////////////////////////////////////////////////////////////////

/// <summary>
/// 通过iovec接口发送多个内存块
/// </summary>
class BufferSendMsg : public BufferList {
public:
    /**
     * 考虑到不同平台的合并写操作不同
    */

    AVC_STATIC_CREATOR(BufferSendMsg)

    ~BufferSendMsg() {}

    int count() const override {
        return 0;
    }
    bool empty() const override {
        return remain_size_ == 0;
    }

    int send(int fd, int flags) override {
        int n = 0;
#if defined(WIN32)
        do {
            DWORD sent = 0;
            n = WSASend(fd, &iovec_[0], iovec_.size(), (LPDWORD)&sent, flags, 0, 0);
            if (n == SOCKET_ERROR) {
                //发送失败
                return -1;
            }
            n = sent;
            //出错是因为UV_ECANELED，则重新发送
        } while(n < 0 && UV_ECANCELED == get_uv_error());


#endif    
    }
private:
    BufferSendMsg(std::list<std::pair<Buffer::Ptr, bool>>&& data, SendResult sendResult) 
        : data_(std::move(data)), iovec_(data.size()) {
        size_t index = 0;
#if defined(WIN32)
        /**
         * 初始化iovec结构
        */
        for (auto &data : data_) {
            iovec_[index].buf = data.first->data();
            iovec_[index].len = data.first->size();
            remain_size_ += data.first->size();
        }
#endif
    }
private:
    std::list<std::pair<Buffer::Ptr, bool>> data_;
#if defined(WIN32)
    /**
     * Win32使用WSASend合并发送多个Buffer
    */
    std::vector<WSABUF> iovec_;
    size_t remain_size_  = 0;
#endif

};//class BufferSendMsg 
////////////////////////////////////////////////////////////////////////////////////

BufferList::Ptr BufferList::create(std::list<std::pair<Buffer::Ptr,bool>>&& data, 
                                   SendResult sendResult, bool isUdp) {
    if (isUdp) {
        return BufferSendTo::create(std::move(data), sendResult, isUdp);
    }
    return BufferSendMsg::create(std::move(data), sendResult);
    //@Todo： linux平台使用sendmmsg和sendmsg优化
}


}
}


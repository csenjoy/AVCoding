#ifndef NETWORK_BUFFER_H
#define NETWORK_BUFFER_H

#include <memory>
#include <string>

#include "util/Util.h"

namespace avc {
namespace util {

/**
 * 其他is_pointer模板类与特化，处理智能指针类型与指针类型
*/
/**
 * 默认情况不属于指针类型
*/
template<class T>
struct is_pointer : public std::false_type {};
template<class T>
struct is_pointer<std::shared_ptr<T>>: public std::true_type {};
template<class T>
struct is_pointer<std::shared_ptr<const T>> : public std::true_type {};
template<class T>
struct is_pointer<T*> : public std::true_type {};
template<class T>
struct is_pointer<const T*> : public std::true_type {};

class Buffer {
public:
    using Ptr = std::shared_ptr<Buffer>;

    virtual ~Buffer() {}

    virtual char *data() const = 0;
    virtual size_t size() const = 0;
    //virtual std::string toString() = 0;
};//class Buffer

class BufferRaw : public Buffer {
public:
    using Ptr = std::shared_ptr<BufferRaw>;

    AVC_STATIC_CREATOR(BufferRaw)
    ~BufferRaw() {
        delete[] data_;
        data_ = nullptr;
        size_ = 0;
        capacity_ = 0;
    }

    char* data() const override {
        return data_;
    }
    
    virtual size_t size() const {
        return size_;
    }

    inline void assign(const char* buffer, size_t size) {
        /**
         * 确定buffer的大小，如果没有指定的话
        */
        if (size <= 0) {
            size = strlen(buffer);
        }
        //创建内存capacity时，比size大于1，方便写入'\0'
        setCapacity(size + 1);

        /**
         * 拷贝buffer内容
        */
        memcpy(data_, buffer, size);
        data_[size] = '\0';
        setSize(size);
    }

    void setCapacity(size_t capacity) {
        if (data_) {
            do {
                if (capacity > capacity_) {
                    //已存在的内存不够用，则重新分配内存
                    break;
                }

                //内存足够用
                //判断内存在2k以下，则直接复用
                if (capacity_ < 2 * 1024) {
                    return;
                }

                //请求的内存大于已存在内存的一半，也复用内存
                if (2 * capacity > capacity_) {
                    return;
                }
                //其他情况，释放内存重新分配
            } while (false);

            //释放内存
            delete[]data_;
            capacity_ = 0;
        }
        data_ = new char[capacity];
        capacity_ = capacity;
    }

    void setSize(size_t size) {
        if (capacity_ < size) {
            throw std::invalid_argument("BufferRaw::setSize out of range");
        }
        size_ = size;
    }
private:
    /**
     * 拷贝内存块data的内容
    */
    BufferRaw(const char *data, size_t size = 0) {
        assign(data, size);
    }
    /**
     * 申请内存块，大小为capacity，内容未定义
    */
    BufferRaw(size_t capacity = 0) {
        //resize(capacity);
        if (capacity) {
            setCapacity(capacity);
        }  
    }
private:
    char* data_ = nullptr;
    size_t size_ = 0;
    size_t capacity_ = 0;
};//class BufferRaw

template<class C>
class BufferOffset : public Buffer {
public:
    using Ptr = std::shared_ptr<BufferOffset>;
    AVC_STATIC_CREATOR(BufferOffset)

    size_t size() const override {
        return size_;
    }

    char* data() const override {
        return const_cast<char *>(getPointer<C>(data_)->data()) + offset_;
    }
private:
    /**
     *  通过一个内存块构造BufferOffset，可以通过offset与size参数描述
     *  实际使用内存块，默认情况下，实际使用内存块是整个内存块
     * 
    */
    BufferOffset(C&& data, size_t size = 0, size_t offset = 0)
        : data_(std::move(data)) {
        setup(size, offset);
    }

    void setup(size_t size, size_t offset) {
        /**
        * 内存块总大小, 通过offset与size访问此内存块
        */
        auto max_size = getPointer<C>(data_)->size();
        if (offset + size >= max_size) {
            throw std::invalid_argument("BufferOffet::setup invalid param");
        }

        if (!size) {
            //如果没有指定size，自动计算size
            size = max_size - offset;
        }
        size_ = size;
        offset_ = offset;
    }

    /**
     * 获取指针类型容器的地址
     *      因为无法区别容器C的类型是指针还是其他普通类型（如std::string)
     *      因此增加getPointer函数，返回容器的地址，统一访问容器
    */
    template<class T>
    static typename std::enable_if<is_pointer<T>::value, const T &>::type
    getPointer(const T &data) {
        return data;
    }

    /**
     * 获取非指针类型容器的地址
    */
    template<class T>
    static typename std::enable_if<!is_pointer<T>::value, const T*>::type
    getPointer(const T &data) {
        return &data;
    }
private:
    /**
     * C的类型可能是普通类型，例如：std::string
     *  也有可能是智能指针，例如：std::shared_ptr
    */
    C data_;
    /**
     * 实际使用的内存大小
    */
    size_t size_ = 0;
    /**
     * 内存块上的偏移，获取实际使用的内存是从data + offset开始
     * 大小为size
    */
    size_t offset_ = 0;
};//class BufferOffset

using BufferString = BufferOffset<std::string>;

#if 0
class BufferLikeString : public Buffer {
public:
    using Ptr = std::shared_ptr<BufferLikeString>;

    AVC_STATIC_CREATOR(BufferLikeString)
    ~BufferLikeString() {} 

    char* data() const override {
        return (char *)data_.data();
    }

    virtual size_t size() const {
        return data_.size();
    }

private:
    BufferLikeString();
    BufferLikeString(std::string&& data) : data_(move(data)) {}
private:
    std::string data_;
};
#endif

}
}


#endif
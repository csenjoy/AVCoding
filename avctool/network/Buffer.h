#ifndef NETWORK_BUFFER_H
#define NETWORK_BUFFER_H

#include <memory>
#include <string>

#include "util/Util.h"

namespace avc {
namespace util {

/**
 * ����is_pointerģ�������ػ�����������ָ��������ָ������
*/
/**
 * Ĭ�����������ָ������
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
         * ȷ��buffer�Ĵ�С�����û��ָ���Ļ�
        */
        if (size <= 0) {
            size = strlen(buffer);
        }
        //�����ڴ�capacityʱ����size����1������д��'\0'
        setCapacity(size + 1);

        /**
         * ����buffer����
        */
        memcpy(data_, buffer, size);
        data_[size] = '\0';
        setSize(size);
    }

    void setCapacity(size_t capacity) {
        if (data_) {
            do {
                if (capacity > capacity_) {
                    //�Ѵ��ڵ��ڴ治���ã������·����ڴ�
                    break;
                }

                //�ڴ��㹻��
                //�ж��ڴ���2k���£���ֱ�Ӹ���
                if (capacity_ < 2 * 1024) {
                    return;
                }

                //������ڴ�����Ѵ����ڴ��һ�룬Ҳ�����ڴ�
                if (2 * capacity > capacity_) {
                    return;
                }
                //����������ͷ��ڴ����·���
            } while (false);

            //�ͷ��ڴ�
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
     * �����ڴ��data������
    */
    BufferRaw(const char *data, size_t size = 0) {
        assign(data, size);
    }
    /**
     * �����ڴ�飬��СΪcapacity������δ����
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
     *  ͨ��һ���ڴ�鹹��BufferOffset������ͨ��offset��size��������
     *  ʵ��ʹ���ڴ�飬Ĭ������£�ʵ��ʹ���ڴ���������ڴ��
     * 
    */
    BufferOffset(C&& data, size_t size = 0, size_t offset = 0)
        : data_(std::move(data)) {
        setup(size, offset);
    }

    void setup(size_t size, size_t offset) {
        /**
        * �ڴ���ܴ�С, ͨ��offset��size���ʴ��ڴ��
        */
        auto max_size = getPointer<C>(data_)->size();
        if (offset + size >= max_size) {
            throw std::invalid_argument("BufferOffet::setup invalid param");
        }

        if (!size) {
            //���û��ָ��size���Զ�����size
            size = max_size - offset;
        }
        size_ = size;
        offset_ = offset;
    }

    /**
     * ��ȡָ�����������ĵ�ַ
     *      ��Ϊ�޷���������C��������ָ�뻹��������ͨ���ͣ���std::string)
     *      �������getPointer���������������ĵ�ַ��ͳһ��������
    */
    template<class T>
    static typename std::enable_if<is_pointer<T>::value, const T &>::type
    getPointer(const T &data) {
        return data;
    }

    /**
     * ��ȡ��ָ�����������ĵ�ַ
    */
    template<class T>
    static typename std::enable_if<!is_pointer<T>::value, const T*>::type
    getPointer(const T &data) {
        return &data;
    }
private:
    /**
     * C�����Ϳ�������ͨ���ͣ����磺std::string
     *  Ҳ�п���������ָ�룬���磺std::shared_ptr
    */
    C data_;
    /**
     * ʵ��ʹ�õ��ڴ��С
    */
    size_t size_ = 0;
    /**
     * �ڴ���ϵ�ƫ�ƣ���ȡʵ��ʹ�õ��ڴ��Ǵ�data + offset��ʼ
     * ��СΪsize
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
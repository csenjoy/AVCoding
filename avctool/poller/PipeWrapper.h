#ifndef POLLER_PIPEWRAPPER_H
#define POLLER_PIPEWRAPPER_H

namespace avc {
namespace util {

class PipeWrapper {
public:
    using FD = int;

    PipeWrapper();
    ~PipeWrapper();

    FD readFD() const {
        return fd_[0];
    }

    FD writeFD() const {
        return fd_[1];
    }

    int write(const void *buffer, int n);
    int read(void *buffer, int n);

    /**
     * 重新打开管道文件描述符
    */
    void reOpenFD();
    void closeFD();

    bool valid() const {
        return readFD() > 0 && writeFD() > 0;
    }
private:
    /**
     * 管道读/写：fd[0]/fd[1]
    */
    FD fd_[2] = {-1, -1};
};//class PipeWrapper 

}
}



#endif
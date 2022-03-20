#include "conn.h"
using namespace std;

const char* Conn::srcDir;
std::atomic<int> Conn::userCount;
bool Conn::isET;

Conn::Conn(int fd, const sockaddr_in& addr): isClose_(false) {
    init(fd, addr);
}

Conn::~Conn() {
    --userCount;
    close(fd_);
    Close();
    LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
}

bool Conn::GetIsClosed() {
    return isClose_;
}
void Conn::init(int fd, const sockaddr_in& addr) {
    assert(fd > 0);
    ++userCount;
    addr_ = addr;
    fd_ = fd;
    writeBuff_.RetrieveAll();
    readBuff_.RetrieveAll();
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
    //isClose_ = false;
}
void Conn::Close() {
    isClose_ = true;
}

int Conn::GetFd() const {
    return fd_;
}

struct sockaddr_in Conn::GetAddr() const {
    return addr_;
}

const char* Conn::GetIP() const {
    return inet_ntoa(addr_.sin_addr);
}

int Conn::GetPort() const {
    return ntohs(addr_.sin_port);
}

ssize_t Conn::read(int* saveErrono){
    ssize_t len = -1;
    do {
        len = readBuff_.ReadFd(fd_, saveErrono);
        if(len <= 0) break;
    }while(isET);
    return len;
}

ssize_t Conn::write(int* saveErrno) {
    ssize_t len = -1;
    do {
        len = writev(fd_, iov_, iovCnt_);
        if(len < 0) {
            *saveErrno = errno;
            break;
        }
        if(iov_[0].iov_len + iov_[1].iov_len == 0) break; //传输结束    
        else if(static_cast<size_t>(len) > iov_[0].iov_len) {
            iov_[1].iov_base = (uint8_t* )iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if(iov_[0].iov_len) {
                writeBuff_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        }
        else {
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len; 
            iov_[0].iov_len -= len; 
            writeBuff_.Retrieve(len);
        }
    }while(isET || ToWriteBytes() > 10240);
    return len;
}

bool Conn::process() {
    request_.Init();
    if(readBuff_.ReadableBytes() <= 0) {
        return 0;
    }
    else if(request_.parse(readBuff_)) {
        //printf("makeresponse发生段错误111\n");
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
        //printf("1111makeresponse发生段错误111\n");
    }
    else {
        //printf("makeresponse发生段错误222\n");
        response_.Init(srcDir, request_.path(), false, 400);
         //printf("222makeresponse发生段错误222\n");
    }
    //printf("makeresponse发生段错误1\n");
    response_.MakeResponse(writeBuff_);
    //printf("makeresponse发生段错误2\n");
    iov_[0].iov_base = (char* )writeBuff_.Peek();
    iov_[0].iov_len = writeBuff_.ReadableBytes();
    iovCnt_ = 1;

    if(response_.FileLen() > 0 && response_.File()) {
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    }
    //printf("makeresponse发生段错误3\n");
     LOG_DEBUG("filesize:%d, %d  to %d", response_.FileLen() , iovCnt_, ToWriteBytes());
    return 1;
}
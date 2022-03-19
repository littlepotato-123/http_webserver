#ifndef BUFFER_H
#define BUFFER_H
#include <cstring>   //perror
#include <iostream>
#include <unistd.h>  // write
#include <sys/uio.h> //readv
#include <vector> 
#include <atomic>
#include <assert.h>
class Buffer {
public:
    Buffer(int initBuffSize = 1024);
    ~Buffer() = default;

    size_t WritableBytes() const;       
    size_t ReadableBytes() const ;
    size_t PrependableBytes() const;

    const std::uint8_t* Peek() const;
    void EnsureWriteable(size_t len);
    void HasWritten(size_t len);

    void Retrieve(size_t len);
    void RetrieveUntil(const std::uint8_t* end);

    void RetrieveAll() ;
    std::string RetrieveAllToStr();

    const std::uint8_t* BeginWriteConst() const;
    std::uint8_t* BeginWrite();

    void Append(const std::string& str);
    void Append(const std::uint8_t* str, size_t len);
    void Append(const void* data, size_t len);
    void Append(const Buffer& buff);

    ssize_t ReadFd(int fd, int* Errno);
    ssize_t WriteFd(int fd, int* Errno);

private:
    std::uint8_t* BeginPtr_();
    const std::uint8_t* BeginPtr_() const;
    void MakeSpace_(size_t len);

    std::vector<std::uint8_t> buffer_;
    std::atomic<std::size_t> readPos_;
    std::atomic<std::size_t> writePos_;
};

#endif 
 
#ifndef SERVER_H
#define SERVER_H

#include <ctime>
#include <unordered_map>
#include <fcntl.h>       // fcntl()
#include <unistd.h>      // close()
#include <assert.h>
#include <cassert>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "epoller.h"
#include "../timer/heaptimer.h"
#include "../pool/threadpool.h"
#include "../conn/conn.h"
#include "../pool/sqlconnpool.h"
class Server {
public:
    Server( int port, int trigMode, int timeoutMS, bool OptLinger,
            int sqlPort, const char* sqlUser, const  char* sqlPwd,
            const char* dbName, int connPoolNum, int threadNum );

    ~Server();
    void Start();

private:

    void InitEventMode_(int trigMode);
    
    bool InitSocket_(); //socket setsockopt bind listen 

    void AddClient_(int fd, sockaddr_in addr);  //epoll_add
   
    void DealListen_();    //accept
    void DealWrite_(std::weak_ptr<Conn> wkclient); //加入线程池的任务队列
    void DealRead_(std::weak_ptr<Conn> wkclient);  //加入线程池的任务队列

    void SendError_(int fd, const char*info); //断开连接（连接满了）
    void ExtentTime_(std::weak_ptr<Conn> wkclient);
    void CloseConn_(std::weak_ptr<Conn> wkclient); //关闭连接，关闭文件描述符，关闭http连接
    
    void OnRead_(std::weak_ptr<Conn> wkclient); //任务队列中具体内容
    void OnWrite_(std::weak_ptr<Conn> wkclient);//任务队列中具体内容
    void OnProcess(std::weak_ptr<Conn> wkclient);//先看能不能读，不可以就转写

    static const int MAX_FD = 65536;

    static int SetFdNonblock(int fd);

    int port_;
    bool openLinger_;
    int timeoutMS_;  /* 毫秒MS */
    bool isClose_;
    int listenFd_;
    char* srcDir_;
    
    uint32_t listenEvent_;
    uint32_t connEvent_;
   
    std::unique_ptr<HeapTimer> timer_;
    std::unique_ptr<ThreadPool> threadpool_;
    std::unique_ptr<Epoller> epoller_;
    //users_并发问题
    std::unordered_map<int, std::shared_ptr<Conn> > users_;
};


#endif 
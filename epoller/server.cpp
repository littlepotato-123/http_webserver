#include "server.h"
using namespace std;

Server::Server(
            int port, int trigMode, int timeoutMS, bool OptLinger,
            int sqlPort, const char* sqlUser, const  char* sqlPwd,
            const char* dbName, int connPoolNum, int threadNum) :
            port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false),
            timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller())
{
    srcDir_ = getcwd(nullptr, 256);
    assert(srcDir_);
    strncat(srcDir_, "/resources/", 16);
    Conn::userCount = 0;
    Conn::srcDir = srcDir_;

    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);

    InitEventMode_(trigMode);
    if(!InitSocket_()) { isClose_ = true;}
}

Server::~Server() {
    //printf("Server is disconstructing\n");
    close(listenFd_);
    isClose_ = true;
    //free(srcDir_);
    //SqlConnPool::Instance()->ClosePool();
}

void Server::InitEventMode_(int trigMode) {
    listenEvent_ = EPOLLRDHUP;
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;
    switch (trigMode)
    {
    case 0:
        break;
    case 1:
        connEvent_ |= EPOLLET;
        break;
    case 2:
        listenEvent_ |= EPOLLET;
        break;
    case 3:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    Conn::isET = (connEvent_ & EPOLLET);
}

void Server::Start(){
    int timeMS = -1;
    //auto begin_time = clock();
    while(!isClose_){
        
        // auto now_time = clock();
        // auto time_pass = (double)(now_time - begin_time)/CLOCKS_PER_SEC * 1000;
        // //printf("%f\n", time_pass);

        // if( time_pass  >= 1){ 
        //     //printf("time out\n");
        //     isClose_ = 1;
        //     continue;
        // }
        if(timeoutMS_ > 0) {
            timeMS = timer_->GetNextTick();
            //printf("timeMS = %d\n", timeMS);
        }

        int eventCnt = epoller_->Wait(timeMS);
        //printf("one more loop\n");

        for(int i = 0; i < eventCnt; ++i){
            int fd = epoller_->GetEventFd(i);
            uint32_t events = epoller_->GetEvents(i);
            if(fd == listenFd_) {
                DealListen_();//<-----
            }
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                //assert(users_.count(fd) > 0);
                CloseConn_(weak_ptr<Conn>(users_[fd]));
            }
            else if(events & EPOLLIN) {
                //assert(users_.count(fd) > 0);
                DealRead_(weak_ptr<Conn>(users_[fd]));
            }
            else if(events & EPOLLOUT) {
               // assert(users_.count(fd) > 0);
                DealWrite_(weak_ptr<Conn>(users_[fd]));
            } 
        }
    }
}

void Server::SendError_(int fd, const char*info) {
    assert(fd > 0);
    send(fd, info, strlen(info), 0);
    close(fd);
}

void Server::CloseConn_(weak_ptr<Conn> wkclient) {
    //assert(client);
    int fd = -1;
    if(auto client = wkclient.lock()){
        if(client->GetIsClosed()) return;
        client->Close();
        fd = client->GetFd();
        //printf("closing client:%d ip:%s port:%d==================\n",client->GetFd(), client->GetIP(), client->GetPort());
        SendError_(client->GetFd(), "time out!");
        epoller_->DelFd(client->GetFd());
        timer_->del_fd(client->GetFd());
        users_.erase(client->GetFd());
    }
    //if(wkclient.expired() && fd != -1) //printf("已经删除连接%d\n", fd);
}

void Server::AddClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    if(users_.count(fd)){
        //printf("same fd\n");
        CloseConn_(weak_ptr<Conn>(users_[fd]));
    }
    users_[fd] = make_shared<Conn>(fd, addr);

    if(timeoutMS_ > 0) {
        timer_->add(fd, timeoutMS_, std::bind(&Server::CloseConn_, this, weak_ptr<Conn>(users_[fd])));
    }
    epoller_->AddFd(fd, EPOLLIN | connEvent_);
    SetFdNonblock(fd);
}

void Server::DealListen_(){
    //printf("deal listen\n");
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do{
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
        if(fd <= 0){return ;}
        else if(Conn::userCount >= MAX_FD){
            SendError_(fd, "Server busy!");
            return;
        }
        //printf("deal listen new connect fd = %d\n", fd);
        AddClient_(fd, addr);
        //printf("deal listen addclient over\n");
    }while(listenEvent_ & EPOLLET); 
}

void Server::DealRead_(weak_ptr<Conn> wkclient){

    //assert(client);
    if(auto client = wkclient.lock()){
        //printf("start to deal read\n");
        ExtentTime_(client);
        threadpool_->AddTask(std::bind(&Server::OnRead_, this, weak_ptr<Conn>(client)));
    }
}

void Server::DealWrite_(weak_ptr<Conn> wkclient) {
   // assert(client);
    if(auto client = wkclient.lock()){
        //printf("start to deal write\n");
        ExtentTime_(client);
        threadpool_->AddTask(std::bind(&Server::OnWrite_, this, weak_ptr<Conn>(client)));
    }
}
void Server::OnRead_(weak_ptr<Conn> wkclient) {
    //assert(client);
    auto client = wkclient.lock();
    if(!client) return;
    //printf("onReading\n");
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);
    if(ret <= 0 && readErrno != EAGAIN) {
        CloseConn_(client);
        return;
    }
    //printf("onRead over, client's fd is %d,begin to on process\n", client->GetFd());
    OnProcess(client);
    ////printf("11发生段错误\n");
}

void Server::OnProcess(weak_ptr<Conn> wkclient) {
    auto client = wkclient.lock();
    ////printf("1发生段错误1\n");
    if(!client) return;
    if(client->process()) {
        ////printf("1发生段错误\n");
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
        ////printf("2发生段错误\n");
    } else {
       // //printf("3发生段错误\n");
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
        ////printf("4发生段错误\n");
    }
}

void Server::OnWrite_(weak_ptr<Conn> wkclient) {
   // assert(client);
    auto client = wkclient.lock();
    if(!client) return;
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if(client->ToWriteBytes() == 0) {
        /* 传输完成 */
        if(client->IsKeepAlive()) {
            OnProcess(weak_ptr<Conn>(client));
            return;
        }
    }
    else if(ret < 0) {
        if(writeErrno == EAGAIN) {
            /* 继续传输 */
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(weak_ptr<Conn>(client));
}
bool Server::InitSocket_(){
    int ret;
    struct sockaddr_in addr;
    if(port_ > 65535 || port_ < 1024){
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);
    struct linger optLinger = {0};
    if(openLinger_){
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenFd_ < 0){
        return false;
    }

    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0){
        close(listenFd_);
        return false;
    }

    int optval = 1;
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));
    if(ret == -1){
        close(listenFd_);
        return false;
    }

    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0){
        close(listenFd_);
        return false;
    }

    ret = listen(listenFd_, 6);
    if(ret < 0){
        close(listenFd_);
        return false;
    }
    ret = epoller_->AddFd(listenFd_, listenEvent_ | EPOLLIN);
    if(ret == 0){
        close(listenFd_);
        return false;
    }
    SetFdNonblock(listenFd_);
    return true;
}

int Server::SetFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}

void Server::ExtentTime_(weak_ptr<Conn> wkclient) {
    //assert(client);
    auto client = wkclient.lock();
    if(!client) return;
    //printf("extentTiming\n");
    if(timeoutMS_ > 0) { timer_->adjust(client->GetFd(), timeoutMS_); }
    //printf("extentTime over\n");
}
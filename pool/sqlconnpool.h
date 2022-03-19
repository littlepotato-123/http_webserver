#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <condition_variable>
#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include <assert.h>

class SqlConnPool {
public:
    static SqlConnPool* Instance();
    ~SqlConnPool();
    MYSQL *GetConn();
   void FreeConn(MYSQL * conn);
   int GetFreeConnCount();

    void Init(const char* host, int port,
              const char* user,const char* pwd, 
              const char* dbName, int connSize);
    void ClosePool();
    //MYSQL* sql_conn = nullptr;
private:
    SqlConnPool() = default;
    
    static SqlConnPool sqlconnpool_;

    int max_size = 0;
    bool is_closing = false;
    
    std::queue<MYSQL *> connQue_;
    std::condition_variable full_, not_empty_;
    std::mutex mtx_;
    sem_t semId_;
};


#endif // SQLCONNPOOL_H

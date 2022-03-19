#ifndef SQLCONNRAII_H
#define SQLCONNRAII_H

#include "sqlconnpool.h"

class sqlconnRAII
{
private:
    MYSQL* sql_conn_ = nullptr;
public:
    sqlconnRAII(MYSQL** sql) {
        sql_conn_ = SqlConnPool::Instance()->GetConn();
        *sql = sql_conn_;
    }
    ~sqlconnRAII() {
        if(sql_conn_) {
            SqlConnPool::Instance()->FreeConn(sql_conn_);
        }
    }
};
#endif
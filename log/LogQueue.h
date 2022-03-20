#ifndef LOGQUEUE_H
#define LOGQUEUE_H

#include <mutex>
#include <queue>
#include <condition_variable>

class LogQueue {
private:
    std::queue<std::string> logs;
    int maxQueueSize_;
    bool isClosed ;
    std::mutex mtx_;
    std::condition_variable producer, consumer;
public:
    LogQueue(int maxQueueSize): maxQueueSize_(maxQueueSize) {
        isClosed = false;
    }
    ~LogQueue() {
        
        Close();
    }

    void Close() {
        {
            std::lock_guard<std::mutex> locker(mtx_);
            isClosed = true;
        }
        consumer.notify_all();
        producer.notify_all();
    }

    int size() {
        std::lock_guard<std::mutex> locker(mtx_);
        return logs.size();
    };

    void pop(std::string& s , int& QueueSize) {
        {
            std::unique_lock<std::mutex> locker(mtx_);
            while(!logs.size()) {
                consumer.wait(locker);
            }
            if(isClosed) return;
            s = std::move(logs.front());
            logs.pop();
            QueueSize = logs.size();    
        }
        producer.notify_one(); 
    }

    void push_back(std::string s, int& QueueSize) {
        {
            std::unique_lock<std::mutex> locker(mtx_);
            while(logs.size() >= maxQueueSize_) {
                producer.wait(locker);
            }
            if(isClosed) return;
            logs.push(std::move(s));
            QueueSize = logs.size();
        }
        consumer.notify_one();
    }
};

#endif
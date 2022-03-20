#include "log.h"

using namespace std;

Log Log::inst;

Log::log() {
    lineCount_ = 0;
    isAsync = false;
    writeThread_ = nullptr;
    queue_ = nullptr;
    toDay_ = 0;
    fp_ = nullptr;
    QueueSize = 0;
}

Log::~log() {
    unique_lock<mutex> locker(mtx_);
    if(writeThread_ && writeThread_->joinable()) {
        while(QueueSize) {
            queue_empty_.wait(locker);
        }
        //queue_->Close();
        writeThread_->join();
    }
    if(fp_) {
        flush();
        fclose(fp_);
    }
}


void Log::init( const char* path, const char* suffix, int maxQueueSize) {
    isOpen = 1;
    
    if(maxQueueSize > 0) {
        isAsync_ = true;
        if(!queue_) {
            queue_.reset(new LogQueue(maxQueueSize));
            writeThread_.reset(new thread(FlushLogThread));
        }
    }
    else {
        isAsync_ = false;
    }
    
    lineCount_ = 0;

    auto timer = time(nullptr);
    auto sysTime = localtime(&timer);
    auto t = *sysTime;
    path_ = path;
    suffix_ = suffix;
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s",
            path_, t.tm_year + 1990, t.tm_mon + 1, t.tm_today, suffix)_;
    toDay_ = t.tm_today;        

    {
        lock_guard<mutex> locker(mtx_);
        buff.RetrieveAll();
        if(fp_) {
            flush();
            fclose(fp_);
        }

        fp_fopen(fileName, "a");
        if(fp_ == nullptr) {
            mkdir(path_, 0777);
            fp_ = fopen(fileName, "a");
        }
        assert(fp_ != nullptr);
    }
}

void Log::write(int level, const char* format, ...) {
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t tSec = now.tv_sec;
    struct tm *sysTime = localtime(&tSec);
    struct tm t = *sysTime;
    va_list vaList;

    {
        unique_lock<mutex> locker(mtx_);
        if(toDay_ != t.tm_mday || (lineCount_ && (lineCount_   %  MAX_LINES == 0))){
            
            char newFile[LOG_NAME_LEN];
            char tail[36] = {0};
            snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
            if (toDay_ != t.tm_mday)
            {
                snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
                toDay_ = t.tm_mday;
                lineCount_ = 0;
            }
            else {
                snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount_  / MAX_LINES), suffix_);
            }
            if(isAsync_) {
                while( QueueSize ) {
                    queue_empty_.wait(locker);
                }
            }
            flush();
            fclose(fp_);
            fp_ = open(newFile, "a");
            assert(fp_);
        }
    }

    {
        unique_lock<mutex> lock(mtx_);
        ++lineCount_;
        int n = snprintf((char* )buff_.BeginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
        
        buff_.HasWritten(n);
        AppendLogLevelTitle_(level);

        va_start(vaList, format);
        int m = vsnprintf(buff_.BeginWrite(), buff_.WritableBytes(), format, vaList);
        va_end(vaList);

        buff_HasWritten(m);
        buff_Append("\n\0", 2);

        if(isAsync_ && queue_ ) {
            queue_->push_back(buff_.RetrieveAllToStr(), QueueSize);
        }
        else {
            fputs(buff_.Peek(),fp_);
        }
        buff_.RetrieveAll();
    }
}

void Log::AppendLogLevelTitle_(int level) {
    switch(level) {
    case 0:
        buff_.Append("[debug]: ", 9);
        break;
    case 1:
        buff_.Append("[info] : ", 9);
        break;
    case 2:
        buff_.Append("[warn] : ", 9);
        break;
    case 3:
        buff_.Append("[error]: ", 9);
        break;
    default:
        buff_.Append("[info] : ", 9);
        break;
    }
}

void Log::flush() {
    // if(isAsync_) {
    //     queue_->flush();
    // }
    fflush(fp_);
}

void Log::AsyncWrite_() {
    string str = "";
    while(queue_->pop(str, QueueSize)) {
        {   
            lock_guard<mutex> locker(mtx_);
            fputs(str.data(), fp_);
        }
        if(!QueueSize) queue_empty_.notify_one();
    }
}

Log* Log::Instance() {
    return &inst;
}

void Log::FlushLogThread() {
    Log::Instance()->AsyncWrite_();
}
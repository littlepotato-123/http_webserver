#include "heaptimer.h"

void HeapTimer::siftup_(size_t child) {
    assert(child >= 0 && child < heap_.size());
    size_t parent = (child - 1) >> 1;
    while(parent >= 0) {
        if(heap_[parent] < heap_[child]) break;
        SwapNode_(child, parent);
        child = parent;
        parent = (child - 1) >> 1;
    } 
}

void HeapTimer::SwapNode_(size_t a, size_t b) {
    assert(a >= 0 && a < heap_.size());
    assert(b >= 0 && b < heap_.size());  
    std::swap(heap_[a], heap_[b]);
    ref_[heap_[a].id] = a;
    ref_[heap_[b].id] = b;
}

bool HeapTimer::siftdown_(size_t index, size_t n) {
    assert(index >= 0 && index < heap_.size());
    assert(n >= 0 && n <= heap_.size());
    size_t parent = index;
    size_t child = (index << 1) + 1;
    //printf("begin to siftdown-\n");
    //printf("parent is %ld, child is %ld\n", parent, child);
    while(child < n){
        if(child + 1 < heap_.size() && heap_[child + 1] < heap_[child])
            ++child;
        if(heap_[parent] < heap_[child]) break;
        SwapNode_(child, parent);
        parent = child;
        child = (parent << 1) + 1;
    }
    //printf("over siftdown-\n");
    return parent > index;
}



void HeapTimer::add(int id, int timeout, const TimeoutCallBack& cb) {
    assert(id >= 0);
    std::lock_guard<std::mutex> locker(mtx_);
    size_t insert_point;
    if(ref_.count(id) == 0) {
        //新节点，堆尾插入，调整堆
        insert_point = heap_.size();
        ref_[id] = insert_point;
        heap_.push_back({id, Clock::now() + MS(timeout), cb});
        siftup_(insert_point);
    }
    else {
        insert_point = ref_[id];
        heap_[insert_point].expires = Clock::now() + MS(timeout) ;
        heap_[insert_point].cb = cb;
        if(!siftdown_(insert_point, heap_.size())) {
            siftup_(insert_point);
        }
    }
}

// void HeapTimer::doWork(int id) {
//     if(heap_.empty() || ref_.count(id) == 0) {
//         return;
//     }
//     size_t i = ref_[id];
//     TimerNode node = heap_[i];
//     node.cb();
//     del_(i);
// }

void HeapTimer::del_fd(int fd) {
    std::lock_guard<std::mutex> locker(mtx_);
    if(!ref_.count(fd)) return;
    del_(ref_[fd]);
}

void HeapTimer::del_(size_t index) {
    assert(!heap_.empty() && index >= 0 && index < heap_.size());
    size_t i = index;
    size_t n = heap_.size() - 1;
    assert(i <= n);
    if(i != n ) SwapNode_(i, n); //交换完 pop掉之后再重组
    //printf("remove timenode:%d, remain timenodes %ld<<<<<<<<<<<<<>>>>>>>>>>>>\n ", heap_.back().id, heap_.size());
    ref_.erase(heap_.back().id);
    heap_.pop_back();

    if(!heap_.size()) return;
    if(i != n) {
        if(!siftdown_(i, n)) siftup_(i);
    }
}

void HeapTimer::adjust(int id, int timeout) {
    assert(!heap_.empty() && ref_.count(id) > 0);
    std::lock_guard<std::mutex> locker(mtx_);
    heap_[ref_[id]].expires = Clock::now() + MS(timeout);
    siftdown_(ref_[id], heap_.size());
}

void HeapTimer::tick() {
    if(heap_.empty()) {
        return;
    }
    while(!heap_.empty()) {
        TimerNode node = heap_.front();
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) {
            break;
        }
        node.cb();
        pop();
    }
}

void HeapTimer::pop() {
    assert(!heap_.empty());
    del_(0);
}

void HeapTimer::clear() {
    ref_.clear();
    heap_.clear();
}

int HeapTimer::GetNextTick() {
    std::lock_guard<std::mutex> locker(mtx_);
    tick();
    size_t res = -1;
    if(!heap_.empty()) {
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if(res < 0) res = 0;
    }
    return res;
}
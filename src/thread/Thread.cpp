/**
 * @Description TODO
 * @Version 1.0.0
 * @Date 2025/2/18 21:03
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */

#include "Thread.h"

// 初始化静态线程ID生成器
std::atomic_int Thread::nextId_(0);

Thread::Thread(ThreadFunc func) : func_(func), threadId_(nextId_++), started_(false){
    // 构造时分配一个唯一的线程ID
}

Thread::~Thread() {
    if (thread_.joinable()) {
        thread_.detach(); // 仅分离，生命周期由线程池管理
    }
}


void Thread::start() {
    // 启动线程，创建 std::thread 对象，将线程函数与线程ID传入
    thread_ = std::thread(func_, threadId_);
    started_ = true;
}

int Thread::getId() const {
    return threadId_;
}

bool Thread::isStarted() const {
    return started_;
}

std::thread &Thread::getThread() {
    return thread_;
}

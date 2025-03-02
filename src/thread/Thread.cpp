/**
 * @Description TODO
 * @Version 1.0.0
 * @Date 2025/2/18 21:03
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */

#include "Thread.h"

int Thread::nextId_ = 0;

Thread::Thread(ThreadFunc func) : func_(func),threadId_(nextId_++){
    // 构造时分配一个唯一的线程ID
}

void Thread::start() {
    // 创建一个新的线程对象，并传入线程函数和当前线程ID
    std::thread t(func_, threadId_);
    t.detach(); // 设置分离线程
}

int Thread::getId() const {
    return threadId_;
}
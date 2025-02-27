/**
 * @Description TODO
 * @Version 1.0.0
 * @Date 2025/2/18 21:03
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */

#include "Thread.h"

int Thread::generateId_ = 0;

Thread::Thread(ThreadFunc func) : func_(func),threadId_(generateId_++){

}

void Thread::start() {
    // 创建一个线程来执行一个线程函数 pthread_create
    std::thread t(func_, threadId_);  // C++11来说 线程对象t  和线程函数func_
    t.detach(); // 设置分离线程   pthread_detach  pthread_t设置成分离线程
}

int Thread::getId() const {
    return threadId_;
}
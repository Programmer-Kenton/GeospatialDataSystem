/**
 * @Description 线程类
 * @Version 1.0.0
 * @Date 2025/2/18 21:03
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */
#ifndef GEOSPATIALDATASYSTEM_THREAD_H
#define GEOSPATIALDATASYSTEM_THREAD_H

#include <iostream>
#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <thread>
#include <future>

class Thread {
public:

    // 线程函数对象类型
    using ThreadFunc = std::function<void(int)>;

    // 线程构造
    Thread(ThreadFunc func);

    // 析构函数
    ~Thread() = default;

    // 启动线程
    void start();

    // 获取线程Id
    int getId() const;

private:
    ThreadFunc func_;
    static int generateId_;
    int threadId_; // 保存线程ID
    std::thread thread_; // 添加线程对象
};


#endif //GEOSPATIALDATASYSTEM_THREAD_H

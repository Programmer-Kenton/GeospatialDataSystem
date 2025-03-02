/**
 * @Description 线程类封装单个工作线程
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

    // 线程函数对象类型 参数为线程ID
    using ThreadFunc = std::function<void(int)>;

    // 构造函数 传入线程函数
    explicit Thread(ThreadFunc func);

    // 析构函数
    ~Thread() = default;

    // 启动线程 内部调用 std::thread 并分离线程
    void start();

    // 获取线程Id
    int getId() const;

private:
    ThreadFunc func_; // 线程执行的函数
    int threadId_;       // 当前线程ID
    static int nextId_;  // 用于生成唯一线程ID的静态变量
};


#endif //GEOSPATIALDATASYSTEM_THREAD_H

/**
 * @Description 定时任务类
 * @Version 1.0.0
 * @Date 2025/2/13 17:22
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */
#ifndef GEOSPATIALDATASYSTEM_GEOTASK_H
#define GEOSPATIALDATASYSTEM_GEOTASK_H

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <functional>

class GeoTask {

private:
    std::thread worker; // 定时工作线程

    std::atomic<bool> running; // 线程运行状态标志

    std::atomic<int> count = 0; // 记录这是第几次执行CSV定时任务

    void timeTask(); // 定时任务

public:
    // 默认构造 线程不启动
    GeoTask();

    ~GeoTask();

    // 启动定时任务
    void start();

    // 停止定时器
    void stop();
};


#endif //GEOSPATIALDATASYSTEM_GEOTASK_H

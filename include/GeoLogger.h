/**
 * @Description 地理信息数据集系统通用日志类
 * @Version 1.0.0
 * @Date 2025/2/18 15:30
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */
#ifndef GEOSPATIALDATASYSTEM_GEOLOGGER_H
#define GEOSPATIALDATASYSTEM_GEOLOGGER_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <unordered_map>
#include <functional>
#include <iomanip>

// 日志级别
enum LogLevel{
    INFO,
    DEBUG,
    WARNING,
    ERROR,
    CRITICAL
};


class GeoLogger {
private:

    std::string logFile; // 日志文件的路径
    bool asyncLogging; // 是否为异步日志
    std::atomic<bool> stopLogging; // 标志位，用于控制日志线程的停止
    LogLevel currentLogLevel = INFO; // 当前日志级别，默认INFO

    // 异步线程在空闲时读取并写入日志文件 主线程不会被阻塞

    std::queue<std::string> logQueue; // 存储日志消息
    std::mutex queueMutex; // 用于保护日志队列的互斥锁
    std::condition_variable cv;  // 用于通知日志线程处理日志队列
    std::thread logThread; // 异步日志线程

private:
    // 日志循环函数：用于异步日志的处理
    void logLoop();

    void writeLog(const std::string& message);

    // 获取当前的时间戳
    std::string getCurrentTime();

    // 将日志级别转换为字符串表示
    std::string logLevelToString(LogLevel level);

public:
    explicit GeoLogger(std::string logFile = "GeoLog.txt",bool async = true);

    ~GeoLogger();

    // 设置日志级别 日志系统只会记录大于或等于此级别的日志。
    void setLogLevel(LogLevel level);

    // 写入日志
    void log(LogLevel level,const std::string& message);
};


#endif //GEOSPATIALDATASYSTEM_GEOLOGGER_H

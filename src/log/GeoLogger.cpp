/**
 * @Description TODO
 * @Version 1.0.0
 * @Date 2025/2/18 15:30
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */
#include "GeoLogger.h"

#include <utility>

GeoLogger::GeoLogger(std::string logFile, bool async) : logFile(std::move(logFile)),asyncLogging(async),stopLogging(false){
    // 如果启用了异步日志就启动一个日志线程
    if (asyncLogging){
        logThread = std::thread(&GeoLogger::logLoop,this); // 启动异步日志线程
    }
}

void GeoLogger::logLoop() {
    while (!stopLogging){
        std::string message;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            cv.wait(lock,[this]{
                return !logQueue.empty() || stopLogging;
            });

            if (stopLogging && logQueue.empty()) break;

            message = logQueue.front();
            logQueue.pop(); // 取出队列中的日志
        }

        writeLog(message); // 异步写入日志
    }
}

void GeoLogger::writeLog(const std::string& message) {
    std::ofstream file(logFile,std::ios::app);
    if (file.is_open()){
        file << message << std::endl;
    } else{
        std::cerr << "无法打开日志系统:" << logFile << std::endl;
    }
}

std::string GeoLogger::getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time),"%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string GeoLogger::logLevelToString(LogLevel level) {
    static std::unordered_map<LogLevel,std::string> logLevelStrings = {
            {INFO,"INFO"},
            {DEBUG,"DEBUG"},
            {WARNING,"WARNING"},
            {ERROR,"ERROR"},
            {CRITICAL,"CRITICAL"}
    };
    return logLevelStrings[level];
}

GeoLogger::~GeoLogger() {
    // 设置停止标志
    stopLogging = true;
    cv.notify_all(); // 通知日志线程退出
    if (logThread.joinable()){
        logThread.join(); // 等待日志完成
    }
}

void GeoLogger::setLogLevel(LogLevel level) {
    currentLogLevel = level;
}

void GeoLogger::log(LogLevel level,const std::string &message) {
    if (level < currentLogLevel) return; // 如果日志级别低于当前设置的级别则跳过

    std::ostringstream oss;
    oss << getCurrentTime() << " [" << logLevelToString(level) << "] " << message;

    // 异步日志：将日志加入队列，由另一个线程异步写入
    if (asyncLogging){
        {
            std::lock_guard<std::mutex> lockGuard(queueMutex);
            logQueue.push(oss.str());
        }
        cv.notify_all(); // 通知日志线程处理队列中的日志
    } else{
        writeLog(oss.str()); // 同步写入日志
    }
}

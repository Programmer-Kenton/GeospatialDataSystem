/**
 * @Description TODO
 * @Version 1.0.0
 * @Date 2025/2/18 21:21
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */
#include "ThreadPool.h"



ThreadPool::ThreadPool()
        : currentThreadCount_(0),
          idleThreadCount_(0),
          taskQueueMaxThreshold_(DEFAULT_TASK_QUEUE_MAX_THRESHOLD),
          threadSizeThreshold_(DEFAULT_THREAD_SIZE_THRESHOLD),
          poolMode_(PoolMode::MODE_FIXED),
          running_(false) {
    // 构造时不启动线程池
}

ThreadPool::~ThreadPool() {
    running_ = false;
    // 通知所有线程退出
    taskNotEmpty_.notify_all();
    // 等待所有线程退出
    std::unique_lock<std::mutex> lock(queueMutex_);
    while (!threads_.empty()) {
        // 这里等待所有线程结束后销毁
        taskNotEmpty_.wait(lock);
    }
}

void ThreadPool::setMode(PoolMode mode) {
    if (running_) return;
    poolMode_ = mode;
}

void ThreadPool::setTaskQueueMaxThreshold(int threshold) {
    if (running_) return;
    taskQueueMaxThreshold_ = threshold;
}

void ThreadPool::setThreadSizeThreshold(int threshold) {
    if (running_) return;
    if (poolMode_ == PoolMode::MODE_CACHED) {
        threadSizeThreshold_ = threshold;
    }
}

void ThreadPool::start(int initThreadCount) {
    running_ = true;
    currentThreadCount_ = initThreadCount;
    idleThreadCount_ = initThreadCount;

    // 创建初始线程
    for (int i = 0; i < initThreadCount; i++) {
        // 创建线程对象，线程函数绑定到 threadFunc，传入线程ID
        auto t = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        int threadId = t->getId();
        threads_.emplace(threadId, std::move(t));
    }
    // 启动所有线程
    for (auto &pair : threads_) {
        pair.second->start();
    }
}

void ThreadPool::threadFunc(int threadId) {
    // 每个线程不断循环，等待任务并执行
    while (running_) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            // 当任务队列为空时，等待任务到来或超时（1秒）
            while (taskQueue_.empty() && running_) {
                if (taskNotEmpty_.wait_for(lock, std::chrono::seconds(1)) == std::cv_status::timeout) {
                    // 动态扩展模式下，超时后如果线程数量超过初始数量则退出回收
                    if (poolMode_ == PoolMode::MODE_CACHED && currentThreadCount_ > static_cast<int>(std::thread::hardware_concurrency())) {
                        currentThreadCount_--;
                        // 线程退出前打印信息
                        std::cout << "线程 " << threadId << " 超时退出" << std::endl;
                        return;
                    }
                }
            }
            if (!running_) {
                return;
            }
            // 从任务队列中取出一个任务
            task = taskQueue_.front();
            taskQueue_.pop();
            // 通知可能正在等待任务队列不满的线程
            taskNotFull_.notify_one();
        } // 锁在此处释放

        // 执行任务
        if (task) {
            task();
        }
    }
}


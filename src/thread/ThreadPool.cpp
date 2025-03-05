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
          running_(false),
          minThreadCount_(0) {
    // 构造时不启动线程池，可通过 start() 后再运行
}

ThreadPool::~ThreadPool() {
    // 停止线程池运行
    running_ = false;
    // 通知所有等待任务的线程退出
    taskNotEmpty_.notify_all();

    std::unique_lock<std::mutex> lock(threadMutex_);
    for (auto &pair : threads_) {
        if (pair.second->getThread().joinable()) {
            pair.second->getThread().join();
        }
    }
    threads_.clear();
}

void ThreadPool::setMode(PoolMode mode) {
    if (!running_) poolMode_ = mode;
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
    // 保存初始线程数，作为动态模式下的最小保留线程数
    minThreadCount_ = initThreadCount;
    currentThreadCount_ = initThreadCount;
    idleThreadCount_ = initThreadCount;

    auto start = std::chrono::high_resolution_clock::now();

    // 创建初始线程并保存到线程集合中
    for (int i = 0; i < initThreadCount; i++) {
        auto t = std::make_unique<Thread>(
                std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1)
        );
        int tid = t->getId();
        std::lock_guard<std::mutex> lock(threadMutex_);
        threads_[tid] = std::move(t);
        threads_[tid]->start();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    logger.log(DEBUG, "创建线程池并启动成功，耗时: " + std::to_string(duration.count()) + "初始线程数量为: " + std::to_string(initThreadCount));
}

void ThreadPool::threadFunc(int threadId) {
    // 记录当前线程的最后活动时间
    auto lastActiveTime = std::chrono::steady_clock::now();

    // 线程主循环，不断从任务队列中取任务执行
    while (running_) {
        Task task;

        {
            // 加锁保护任务队列
            std::unique_lock<std::mutex> lock(queueMutex_);

            // 当前线程进入空闲状态，空闲线程计数+1
            idleThreadCount_++;

            // 带超时的条件等待：当任务队列为空且线程池仍在运行时
            while (taskQueue_.empty() && running_) {
                // 等待任务到来，最多等待1秒
                if (taskNotEmpty_.wait_for(lock, std::chrono::seconds(1)) == std::cv_status::timeout) {
                    // 如果等待超时，检查是否需要缩容
                    auto now = std::chrono::steady_clock::now();
                    auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastActiveTime);

                    // 动态缩容条件：
                    // 1. 线程池模式为 MODE_CACHED
                    // 2. 当前线程空闲时间超过1秒
                    // 3. 当前线程数大于最小保留线程数
                    if (poolMode_ == PoolMode::MODE_CACHED &&
                        dur.count() >= 1 &&
                        currentThreadCount_ > minThreadCount_) {
                        // 减少线程计数
                        currentThreadCount_--;
                        idleThreadCount_--;

                        // 加锁保护线程容器，移除当前线程
                        std::lock_guard<std::mutex> tlock(threadMutex_);
                        threads_.erase(threadId);

                        // 线程退出
                        return;
                    }
                }
            }

            // 如果线程池已停止，退出循环
            if (!running_) break;

            // 如果任务队列为空，跳过本次循环
            if (taskQueue_.empty()) continue;

            // 从任务队列中取出一个任务
            idleThreadCount_--; // 当前线程从空闲状态变为忙碌状态
            task = taskQueue_.front();
            taskQueue_.pop();

            // 通知可能因任务队列满而等待的提交者
            taskNotFull_.notify_one();

            // 更新最后活动时间
            lastActiveTime = std::chrono::steady_clock::now();
        }

        // 执行任务（如果任务有效）
        if (task) {
            task();
        }
    }

    // 线程池停止运行时的安全退出处理
    std::lock_guard<std::mutex> lock(threadMutex_);
    threads_.erase(threadId); // 从线程容器中移除当前线程
}

int ThreadPool::getCurrentThreadCount() const {
    return currentThreadCount_.load();
}


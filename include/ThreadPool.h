/**
 * @Description 通用线程池类
 * @Version 1.0.0
 * @Date 2025/2/18 21:21
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */
#ifndef GEOSPATIALDATASYSTEM_THREADPOOL_H
#define GEOSPATIALDATASYSTEM_THREADPOOL_H

#include "Thread.h"
#include "Public.h"

//const int TASK_MAX_THRESHHOLD = INT32_MAX;
//const int THREAD_MAX_THRESHHOLD = 1024;
//const int THREAD_MAX_IDLE_TIME = 60; // 单位 秒

// 默认上限值
const int DEFAULT_TASK_QUEUE_MAX_THRESHOLD = INT32_MAX;
const int DEFAULT_THREAD_SIZE_THRESHOLD = 2048;
const int THREAD_INIT_COUNT = 10;

// 线程池支持的模式
enum class PoolMode{
    MODE_FIXED, // 固定数量的线程
    MODE_CACHED, // 动态扩展模式
};

class ThreadPool {
public:
    // 线程池构造函数
    ThreadPool();

    // 线程池析构 等待所有线程退出
    ~ThreadPool();

    // 设置线程池的工作模式
    void setMode(PoolMode mode);

    // 设置任务队列的最大阈值
    void setTaskQueueMaxThreshold(int threshold);

    // 设置动态扩展模式下线程总数的上限
    void setThreadSizeThreshold(int threshold);

    // 启动线程池，initThreadCount 默认采用硬件并发数
    void start(int initThreadCount = std::thread::hardware_concurrency());

    // 提交任务到线程池，返回 std::future 对象以便获取任务返回值
    // 线程池扩容逻辑
    // 线程池在 submitTask 方法中检查任务队列是否满了，如果满了，并且：线程池处于 MODE_CACHED 模式（动态扩展模式）。
    // 当前线程数 (currentThreadCount_) 小于最大线程数阈值 (threadSizeThreshold_)。
    template<typename Func, typename... Args>
    auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>{
        using ReturnType = decltype(func(args...)); // 推导函数 func 的返回类型

        // 创建一个 std::packaged_task，封装任务，使其可以异步执行
        auto taskPtr = std::make_shared<std::packaged_task<ReturnType()>>(
                std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
        );

        // 获取任务的 future，用于异步获取返回值
        std::future<ReturnType> future = taskPtr->get_future(); // 这里保持 std::future 类型

        {
            std::unique_lock<std::mutex> lock(queueMutex_); // 加锁保护任务队列

            // 等待任务队列有空间，如果队列已满，最多等待1秒
            if (!taskNotFull_.wait_for(lock, std::chrono::seconds(1), [this] {
                return taskQueue_.size() < (size_t)taskQueueMaxThreshold_;
            })) {

                // 如果线程池是 "缓存模式"（MODE_CACHED），且当前线程数未超过阈值，则创建新线程
                if (poolMode_ == PoolMode::MODE_CACHED && currentThreadCount_ < threadSizeThreshold_) {
                    // 创建一个新线程
                    auto newThread = std::make_unique<Thread>(
                            std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1)
                    );
                    int tid = newThread->getId();
                    {
                        std::lock_guard<std::mutex> tlock(threadMutex_); // 保护线程池结构的修改
                        threads_[tid] = std::move(newThread); // 将新线程加入线程池
                        currentThreadCount_++; // 增加当前线程计数
                        idleThreadCount_++; // 增加空闲线程计数
                    }
                    threads_[tid]->start(); // 启动新线程
                }

                // 如果任务队列仍然满了，直接返回一个默认的 future，防止程序崩溃
                if (taskQueue_.size() >= (size_t)taskQueueMaxThreshold_) {
                    return std::future<ReturnType>(); // 这里返回一个默认 future，防止崩溃
                }
            }

            // 任务队列仍有空间，将新任务加入任务队列
            taskQueue_.emplace([taskPtr]() { (*taskPtr)(); });
        }

        // 通知等待中的线程，有新任务可执行
        taskNotEmpty_.notify_one();
        return future;  // 返回 std::future
    }


    // 获取当前线程池中线程的数量
    int getCurrentThreadCount() const;

    // 禁止拷贝和赋值
    ThreadPool(const ThreadPool&) = delete;

    ThreadPool &operator=(const ThreadPool&) = delete;

private:
    // 线程池内部的工作线程函数，每个线程不断从任务队列中取任务执行
    void threadFunc(int threadId);

    // 检查线程池是否还在运行
    bool isRunning() const { return running_; }


private:
    // 任务类型，封装为无返回值的 std::function
    using Task = std::function<void()>;

    // 任务队列及其保护互斥锁和条件变量
    std::queue<Task> taskQueue_;
    std::mutex queueMutex_;
    std::condition_variable taskNotEmpty_; // 通知任务队列非空
    std::condition_variable taskNotFull_;    // 通知任务队列未满

    // 线程集合，保存所有工作线程，键为线程ID
    std::unordered_map<int, std::unique_ptr<Thread>> threads_;
    // 用于保护线程集合的互斥锁
    std::mutex threadMutex_;

    // 当前线程总数和空闲线程数（原子变量保证多线程下操作安全）
    std::atomic_int currentThreadCount_;
    std::atomic_int idleThreadCount_;

    // 初始线程数，在动态扩展模式下为最低保留线程数
    int minThreadCount_;

    // 任务队列的最大阈值
    int taskQueueMaxThreshold_;
    // 动态扩展模式下线程总数的上限
    int threadSizeThreshold_;

    // 线程池工作模式
    PoolMode poolMode_;
    // 标记线程池是否正在运行
    std::atomic_bool running_;
};


#endif //GEOSPATIALDATASYSTEM_THREADPOOL_H

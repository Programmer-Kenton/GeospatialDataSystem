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

//const int TASK_MAX_THRESHHOLD = INT32_MAX;
//const int THREAD_MAX_THRESHHOLD = 1024;
//const int THREAD_MAX_IDLE_TIME = 60; // 单位 秒

// 默认上限值
const int DEFAULT_TASK_QUEUE_MAX_THRESHOLD = INT32_MAX;
const int DEFAULT_THREAD_SIZE_THRESHOLD = 2048;
const int THREAD_INIT_COUNT = 1024;

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

    // 设置任务队列上限
    void setTaskQueueMaxThreshold(int threshold);

    // 设置动态扩展模式下线程总数上限
    void setThreadSizeThreshold(int threshold);

    // 启动线程池，initThreadCount 默认采用硬件并发数
    void start(int initThreadCount = std::thread::hardware_concurrency());

    // 提交任务到线程池，返回 std::future 对象以便获取任务返回值
    template<typename Func, typename... Args>
    auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>{
        using ReturnType = decltype(func(args...));
        // 将任务封装成 packaged_task 并获取 future
        auto taskPtr = std::make_shared<std::packaged_task<ReturnType()>>(
                std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
        );
        std::future<ReturnType> future = taskPtr->get_future();

        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            // 如果任务队列已满，则等待（最多1秒）
            if (!taskNotFull_.wait_for(lock, std::chrono::seconds(1), [this]() { return taskQueue_.size() < (size_t)taskQueueMaxThreshold_; })) {
                std::cerr << "任务队列已满，提交任务失败" << std::endl;
                // 立即返回一个空任务的 future
                auto emptyTask = std::make_shared<std::packaged_task<ReturnType()>>([]()->ReturnType { return ReturnType(); });
                return emptyTask->get_future();
            }
            // 将任务加入任务队列中
            taskQueue_.push([taskPtr]() { (*taskPtr)(); });
        }
        // 通知等待任务的线程
        taskNotEmpty_.notify_one();

        return future;
    }

    // 禁止拷贝和赋值
    ThreadPool(const ThreadPool&) = delete;

    ThreadPool &operator=(const ThreadPool&) = delete;

private:
    // 线程池内部的工作线程函数，每个线程不断从任务队列中取任务执行
    void threadFunc(int threadId);

    // 检查线程池是否还在运行
    bool isRunning() const { return running_; }


private:
    // 线程池的任务类型：无返回值函数
    using Task = std::function<void()>;

    // 任务队列
    std::queue<Task> taskQueue_;
    // 用于保护任务队列的互斥锁
    mutable std::mutex queueMutex_;
    // 条件变量，用于等待任务队列非空
    std::condition_variable taskNotEmpty_;
    // 条件变量，用于等待任务队列未满
    std::condition_variable taskNotFull_;

    // 保存所有线程，键为线程ID
    std::unordered_map<int, std::unique_ptr<Thread>> threads_;
    // 当前线程池中线程总数
    std::atomic_int currentThreadCount_;
    // 当前空闲线程数
    std::atomic_int idleThreadCount_;

    // 任务队列的上限阈值
    int taskQueueMaxThreshold_;
    // 动态扩展模式下线程总数的上限
    int threadSizeThreshold_;

    // 线程池工作模式
    PoolMode poolMode_;
    // 线程池是否正在运行
    std::atomic_bool running_;

};


#endif //GEOSPATIALDATASYSTEM_THREADPOOL_H

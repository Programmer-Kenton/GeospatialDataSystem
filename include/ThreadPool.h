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

const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 1024;
const int THREAD_INIT_COUNT = 512;
const int THREAD_MAX_IDLE_TIME = 60; // 单位 秒

// 线程池支持的模式
enum class PoolMode{
    MODE_FIXED, // 固定数量的线程
    MODE_CACHED, // 线程数量可动态增长
};

class ThreadPool {
public:
    // 线程池构造函数
    ThreadPool();

    // 线程池析构
    ~ThreadPool();

    // 设置线程池的工作模式
    void setMode(PoolMode mode);

    // 设置task任务队列上限阈值
    void setTaskQueMaxThreshHold(int threshhold);

    // 设置线程池cached模式下线程阈值
    void setThreadSizeThreshHold(int threshhold);

    // 给线程池提交任务
    // 使用可变参模板编程，让submitTask可以接收任意任务函数和任意数量的参数
    // pool.submitTask(sum1, 10, 20);   csdn  大秦坑王  右值引用+引用折叠原理
    // 返回值future<>
    template<typename Func, typename... Args>
    auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
    {
        // 打包任务，放入任务队列里面
        using RType = decltype(func(args...));
        auto task = std::make_shared<std::packaged_task<RType()>>(
                std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
        std::future<RType> result = task->get_future();

        // 获取锁
        std::unique_lock<std::mutex> lock(taskQueMtx_);
        // 用户提交任务，最长不能阻塞超过1s，否则判断提交任务失败，返回
        if (!notFull_.wait_for(lock, std::chrono::seconds(1),
                               [&]()->bool { return taskQue_.size() < (size_t)taskQueMaxThreshHold_; }))
        {
            // 表示notFull_等待1s种，条件依然没有满足
            std::cerr << "task queue is full, submit task fail." << std::endl;
            auto task = std::make_shared<std::packaged_task<RType()>>(
                    []()->RType { return RType(); });
            (*task)();
            return task->get_future();
        }

        // 如果有空余，把任务放入任务队列中
        // taskQue_.emplace(sp);
        // using Task = std::function<void()>;
        taskQue_.emplace([task]() {(*task)();});
        taskSize_++;

        // 因为新放了任务，任务队列肯定不空了，在notEmpty_上进行通知，赶快分配线程执行任务
        notEmpty_.notify_all();

        // cached模式 任务处理比较紧急 场景：小而快的任务 需要根据任务数量和空闲线程的数量，判断是否需要创建新的线程出来
        if (poolMode_ == PoolMode::MODE_CACHED
            && taskSize_ > idleThreadSize_
            && curThreadSize_ < threadSizeThreshHold_)
        {
            std::cout << ">>> create new thread..." << std::endl;

            // 创建新的线程对象
            auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
            int threadId = ptr->getId();
            threads_.emplace(threadId, std::move(ptr));
            // 启动线程
            threads_[threadId]->start();
            // 修改线程个数相关的变量
            curThreadSize_++;
            idleThreadSize_++;
        }

        // 返回任务的Result对象
        return result;
    }


    // 开启线程池
    void start(int initThreadSize = std::thread::hardware_concurrency());

    ThreadPool(const ThreadPool&) = delete;

    ThreadPool &operator = (const ThreadPool &) = delete;

private:
    // 定义线程函数
    void threadFunc(int threadId);

    // 检查线程池运行状态
    bool checkRunningState() const;

private:
    std::unordered_map<int,std::unique_ptr<Thread>> threads_; // 线程列表

    int initThreadSize_; // 初始的线程数量
    int threadSizeThreshHold_; // 线程数量上限阈值
    std::atomic_int curThreadSize_; // 记录当前线程池里面线程的总数量
    std::atomic_int idleThreadSize_; // 记录空闲线程的数量

    // Task任务 --> 函数对象
    using Task = std::function<void()>;
    std::queue<Task> taskQue_; // 任务队列
    std::atomic_int taskSize_; // 任务的数量
    int taskQueMaxThreshHold_;  // 任务队列数量上限阈值

    std::mutex taskQueMtx_; // 保证任务队列的线程安全
    std::condition_variable notFull_; // 表示任务队列不满
    std::condition_variable notEmpty_; // 表示任务队列不空
    std::condition_variable exitCond_; // 等到线程资源全部回收

    PoolMode poolMode_; // 当前线程池的工作模式
    std::atomic_bool isPoolRunning_; // 表示当前线程池的启动状态
};


#endif //GEOSPATIALDATASYSTEM_THREADPOOL_H

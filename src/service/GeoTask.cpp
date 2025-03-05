/**
 * @Description 定时任务类
 * @Version 1.0.0
 * @Date 2025/2/13 17:22
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */

#include "GeoTask.h"
#include "GeoTools.h"


void GeoTask::timeTask() {
    while (running.load()){
        // std::cout << "定时任务启动: 执行 GeoTools::modifyCSV()" << std::endl;

        GeoTools::modifyCSV(fileName);

        // 使用for循环检查running状态可以随时终止任务线程
        for (int i = 0; i < 1 && running.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::minutes (1));
        }
    }
}

GeoTask::~GeoTask() {
    stop();
}

void GeoTask::start() {
    if (running.load()) return; // 避免重复启动

    running.store(true);
    worker = std::thread(&GeoTask::timeTask,this);

    logger.log(INFO,"定时处理CSV文件任务启动...");
}

void GeoTask::stop() {
    running.store(false);
    if (worker.joinable()){
        worker.join();
    }
}

GeoTask::GeoTask() : running(false){

}

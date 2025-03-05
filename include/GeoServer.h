/**
 * @Description TODO
 * @Version 1.0.0
 * @Date 2025/2/11 23:35
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */
#ifndef GEOSPATIALDATASYSTEM_GEOTOOL_H
#define GEOSPATIALDATASYSTEM_GEOTOOL_H

#include "Public.h"
#include "GeoTools.h"
#include "GeoTask.h"
#include <fstream>
#include <random>
#include <python3.8/Python.h>
//#include <python3.12/Python.h>

class GeoServer {
public:

    // 加载配置
    bool loadConfig();


    // 初始化系统
    bool initSystem();

    // 调用Python脚本生成地理信息数据并插入R树
    bool insertRTreeData(int num, unsigned long long initID);

    // 删除R树中的数据
    bool deleteRtreeData(int num,unsigned long long deleteId = 0);

    // 查询相交的面并统计结果
    // 1. 检查输入坐标是否足够构成多边形（至少3个点）。
    // 2. 确保坐标闭合，构造查询多边形和其最小外接矩形（MBR）。
    // 3. 使用 manager->searchRTree(query_box) 查询 R 树，获得结果向量。
    // 4. 利用线程池对查询结果分段并行处理，每个任务将本段结果转换为 JSON 数组，并统计各类型数量。
    // 5. 主线程等待所有任务完成后，合并各段结果和统计数据，组装最终 JSON 响应。
    nlohmann::json queryRTreePolygon(const std::vector<Point> &coords);

    // 程序结束销毁资源
    void destroySystem();

    // 获取当前R树尺寸
    unsigned long long GetGaoDataCount();

private:
    GeoTask task;

    ThreadPool pool;
};


#endif //GEOSPATIALDATASYSTEM_GEOTOOL_H

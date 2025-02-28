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
//#include <python3.8/Python.h>
#include <python3.12/Python.h>

class GeoServer {
public:

    // 初始化系统
    void initSystem();

    // 调用Python脚本生成地理信息数据并插入R树
    bool insertRTreeData(int num, unsigned long long initID);

    // 删除R树中的数据
    bool deleteRtreeData(unsigned long long maxId,int num,unsigned long long deleteId = 0);

    // 查询相交的面并统计结果
    void queryRTreePolygon();

private:
    GeoTask task;

    ThreadPool pool;
};


#endif //GEOSPATIALDATASYSTEM_GEOTOOL_H

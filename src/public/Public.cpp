/**
 * @Description TODO
 * @Version 1.0.0
 * @Date 2025/2/11 21:58
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */

#include "Public.h"


unsigned long long id = 0;

std::vector<GeoObject> insertVec;
std::mutex insert_mutex;
std::unordered_set<unsigned long long> deleteSet;
std::mutex delete_mutex;
std::unordered_set<unsigned long long> deleteRtreeSet; // 存储R树要删除的id号对应的信息
std::mutex deleteRTree_mutex;
std::string fileName = "/usr/local/WorkSpace/GeospatialDataSystem/data/geospatial_data.csv";
// 初始化日志
GeoLogger logger("GeoLog.txt", true);  // 使用异步日志
httplib::Server server;

// RTreeManager 的初始化
RTreeManager* manager = &RTreeManager::getInstance();
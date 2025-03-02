/**
 * @Description TODO
 * @Version 1.0.0
 * @Date 2025/2/11 21:58
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */
#ifndef GEOSPATIALDATASYSTEM_PUBLIC_H
#define GEOSPATIALDATASYSTEM_PUBLIC_H

#include "GeoObject.h"
#include "RTreeManager.h"
#include "GeoLogger.h"
#include "ThreadPool.h"
#include "httplib.h"
#include <nlohmann/json.hpp>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <string>

extern std::atomic<unsigned long long int> id;
extern std::vector<GeoObject> insertVec; // 存储新生成的地理信息数据
extern std::mutex insert_mutex;
extern std::unordered_set<unsigned long long> deleteSet; // 存储CSV文件要定时删除的id号
extern std::mutex delete_mutex;
extern std::unordered_set<unsigned long long> deleteRtreeSet; // 存储R树要删除的id号对应的信息
extern std::mutex deleteRTree_mutex;
extern std::string fileName; // CSV文件路径
extern GeoLogger logger;
extern httplib::Server server;
extern int deleteCount;

#define POINT "Point"
#define LINE "Line"
#define POLYGON "Polygon"

extern RTreeManager *manager;




#endif //GEOSPATIALDATASYSTEM_PUBLIC_H

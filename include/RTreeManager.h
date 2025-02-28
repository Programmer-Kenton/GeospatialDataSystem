/**
 * @Description R树管理类
 * @Version 1.0.0
 * @Date 2025/2/11 15:21
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */
#ifndef GEOSPATIALDATASYSTEM_RTREEMANAGER_H
#define GEOSPATIALDATASYSTEM_RTREEMANAGER_H

#include "GeoObject.h"
#include "ThreadPool.h"
#include <boost/geometry/index/rtree.hpp>
#include <boost/geometry/algorithms/equals.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>
#include <sstream>
#include <vector>
#include <mutex>
#include <iostream>
#include <shared_mutex>
#include <unordered_set>

namespace bgi = boost::geometry::index;
typedef std::pair<Box,std::string> Value;

class RTreeManager {
private:
    bgi::rtree<Value, bgi::quadratic<64>> rtree; // R树实例
    mutable std::shared_mutex rtree_mutex; // R树线程安全共享锁

    // 私有构造函数 防止外部实例化
    RTreeManager() {}

public:

    // 获取单例实例
    static RTreeManager& getInstance();

    // 删除拷贝构造函数和赋值运算符，防止拷贝实例
    RTreeManager(const RTreeManager&) = delete;
    RTreeManager& operator=(const RTreeManager&) = delete;

    // 构建R树 利用线程池并行化处理和批量插入构建R树
    void buildRTree(const std::vector<GeoObject>& geo_objects,ThreadPool& pool);

    // 查找与外接矩形相交的Value
    std::vector<Value> searchRTree(const Box& queryBox) const;

    // 向R树插入数据
    void insertRTree(const Box& bounding_box, const std::string& str);

    // 删除R树的数据
    bool deleteRTree();

    // 打印R树
    void printRTree() const;

    // 获取R树的尺寸
    unsigned long long count() const{
        return rtree.size();
    }

    std::shared_mutex& getMutex() const{
        return rtree_mutex;
    }

    // 将Box内容转换为字符串返回
    std::string box_to_string(const Box& box);
};


#endif //GEOSPATIALDATASYSTEM_RTREEMANAGER_H

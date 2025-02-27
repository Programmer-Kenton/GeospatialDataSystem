/**
 * @Description TODO
 * @Version 1.0.0
 * @Date 2025/2/11 15:21
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */

#include "RTreeManager.h"
#include "Public.h"


void RTreeManager::buildRTree(const std::vector<GeoObject>& geo_objects,ThreadPool& pool) {
        if (geo_objects.empty()) return; // 避免空数据

        logger.log(DEBUG, "Geo objects size: " + std::to_string(geo_objects.size()));

        // 获取线程池的线程数
        const int numThreads = THREAD_INIT_COUNT;

        // 根据线程数计算每个线程处理的对象数量
        size_t batchSize = geo_objects.size() / numThreads;
        if (geo_objects.size() % numThreads != 0) {
            batchSize++; // 如果数据量无法整除，最后一批处理的对象数量会多
        }

        logger.log(DEBUG, "每个线程的批次大小: " + std::to_string(batchSize));

        logger.log(INFO, "开始使用线程池构建R树，共 " + std::to_string(numThreads) + " 个线程处理数据");

        // 使用线程池并行构建R树
        std::vector<std::future<std::vector<std::pair<Box, std::string>>>> futures; // 线程任务返回局部数据

        // 根据线程数将任务划分成多个批次进行处理
        for (int i = 0; i < numThreads; ++i) {
            size_t startIdx = i * batchSize; // 当前线程处理的起始索引
            size_t endIdx = (i == numThreads - 1) ? geo_objects.size() : startIdx + batchSize; // 当前线程处理的结束索引

            // 确保 endIdx 不大于 geo_objects.size()
            endIdx = std::min(endIdx, geo_objects.size());

            logger.log(DEBUG, "线程 " + std::to_string(i) + " 开始处理索引范围 [" + std::to_string(startIdx) + ", " + std::to_string(endIdx) + "]");

            // 提交任务到线程池，每个线程处理自己的数据批次
            futures.push_back(pool.submitTask([this, &geo_objects, startIdx, endIdx]()->std::vector<std::pair<Box, std::string>> {
                std::vector<std::pair<Box, std::string>> batch_data;  // 每个线程的局部数据

                // 遍历当前线程负责的部分数据
                for (size_t j = startIdx; j < endIdx; ++j) {
                    const auto& obj = geo_objects[j]; // 获取当前 GeoObject
                    if (obj.coordinates.empty()) continue;  // 如果没有坐标数据，跳过

                    Box bounding_box; // 定义外接矩形

                    // 根据 GeoObject 的类型构建相应的外接矩形
                    if (obj.type == POINT) {
                        bounding_box = Box(obj.coordinates.front(), obj.coordinates.front()); // 点的外接矩形就是点本身
                    } else if (obj.type == LINE) {
                        LineString line(obj.coordinates.begin(), obj.coordinates.end()); // 线段
                        bounding_box = bg::return_envelope<Box>(line); // 获取线段的外接矩形
                    } else if (obj.type == POLYGON) {
                        Polygon polygon;
                        std::vector<Point> mutable_coords = obj.coordinates;
                        if (!bg::equals(mutable_coords.front(), mutable_coords.back())) {
                            mutable_coords.push_back(mutable_coords.front());  // 确保多边形闭合
                        }
                        polygon.outer().assign(mutable_coords.begin(), mutable_coords.end());
                        bounding_box = bg::return_envelope<Box>(polygon); // 获取多边形的外接矩形
                    } else {
                        continue; // 如果类型未知，则跳过
                    }

                    batch_data.emplace_back(bounding_box, obj.id + "," + obj.type);
                }

                return batch_data;  // 线程任务返回数据
            }));
        }

        // 统一插入到 R 树，减少锁争用
        std::vector<std::pair<Box, std::string>> all_data;
        std::unordered_set<std::string> seen_ids;  // 记录已经插入的 ID，防止重复

        for (auto& f : futures) {
            auto batch = f.get();
            for (const auto& data : batch) {
                if (seen_ids.insert(data.second).second) { // `insert` 返回 <iterator, bool>，bool=true 表示新插入
                    all_data.push_back(data);
                }
            }
        }

        // 统一加锁批量插入
        {
            std::unique_lock<std::shared_mutex> lock(rtree_mutex);
            rtree.insert(all_data.begin(), all_data.end());
            logger.log(INFO, "R树构建完成，共插入 " + std::to_string(all_data.size()) + " 条数据");
        }
}

std::vector<Value> RTreeManager::searchRTree(const Box& queryBox) const {
    std::vector<Value> results;
    rtree.query(bgi::intersects(queryBox), std::back_inserter(results));
    return results;
}

RTreeManager &RTreeManager::getInstance() {
    static RTreeManager instance;
    return instance;
}

void RTreeManager::printRTree() const {
    std::lock_guard<std::shared_mutex> lock(rtree_mutex); // 确保线程安全

    std::cout << "R树内容:" << std::endl;
    for (const auto &item : rtree) {
        std::cout << "数据: " << item.second << std::endl;
    }
}



bool RTreeManager::deleteRTree() {
    std::vector<Value> to_remove; // 存储待删除项

    {
        std::shared_lock<std::shared_mutex> lock(rtree_mutex); // 共享锁，允许并发读取 R 树

        // 遍历 R 树，查找匹配的 ID
        for (const auto& item : rtree) {
            std::istringstream ss(item.second);
            std::string id_str;
            std::getline(ss, id_str, ','); // 获取 ID 字符串

            try {
                unsigned long long obj_id = std::stoull(id_str);
                if (deleteRtreeSet.find(obj_id) != deleteRtreeSet.end()) {
                    to_remove.push_back(item); // 记录待删除项
                }
            } catch (const std::exception& e) {
                std::cerr << "警告: 无法解析 ID \"" << id_str << "\" - " << e.what() << std::endl;
            }
        }
    } // 释放共享锁

    if (to_remove.empty()) {
        return false; // 无需删除
    }

    {
        std::lock_guard<std::shared_mutex> lock(rtree_mutex); // 独占锁，确保安全删除
        for (const auto& item : to_remove) {
            rtree.remove(item);
        }
    }

    {
        std::lock_guard<std::mutex> lock(deleteRTree_mutex);
        for (const auto& item : to_remove) {
            std::istringstream ss(item.second);
            std::string id_str;
            std::getline(ss, id_str, ','); // 获取 ID 字符串
            try {
                unsigned long long obj_id = std::stoull(id_str);
                deleteRtreeSet.erase(obj_id); // 从集合中移除 ID
            } catch (const std::exception& e) {
                std::cerr << "警告: 删除 ID 时解析失败 \"" << id_str << "\" - " << e.what() << std::endl;
            }
        }
    }

    return true;
}

void RTreeManager::insertRTree(const Box &bounding_box, const std::string &str) {
    std::unique_lock<std::shared_mutex> lock(rtree_mutex); // 独占锁，防止多个线程并发写入
    rtree.insert(std::make_pair(bounding_box, str));
}

std::string RTreeManager::box_to_string(const Box &box) {
    // 获取 box 的最小点和最大点的坐标
    double min_x = bg::get<0>(box.min_corner());
    double min_y = bg::get<1>(box.min_corner());
    double max_x = bg::get<0>(box.max_corner());
    double max_y = bg::get<1>(box.max_corner());

    // 创建字符串输出流
    std::ostringstream oss;

    // 将坐标转换为字符串格式：[(min_x, min_y), (max_x, max_y)]
    oss << "[(" << min_x << ", " << min_y << "), "
        << "(" << max_x << ", " << max_y << ")]";

    // 返回格式化的字符串
    return oss.str();
}



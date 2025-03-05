/**
 * @Description TODO
 * @Version 1.0.0
 * @Date 2025/2/11 15:21
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */

#include "RTreeManager.h"
#include "Public.h"
#include "GeoTools.h"


void RTreeManager::buildRTree(const std::vector<GeoObject> &geo_objects, ThreadPool &pool) {
    if (geo_objects.empty()) return; // 避免空数据

    logger.log(DEBUG, "传入构建R树的geo_objects尺寸为:" + std::to_string(geo_objects.size()));

    // 根据线程数计算每个线程处理的对象数量
    int threadCount = pool.getCurrentThreadCount();
    // 根据 geo_objects 大小，计算每个线程处理的数据数量（向上取整）
    size_t batchSize = (geo_objects.size() + threadCount - 1) / threadCount;  // 等价于向上取整



    logger.log(DEBUG, "每个线程的批次大小: " + std::to_string(batchSize));
    logger.log(INFO, "开始使用线程池构建R树，共 " + std::to_string(threadCount) + " 个线程处理数据");


    // futures 用来保存每个线程返回的局部结果，每个局部结果为一个 vector<pair<Box, string>>
    std::vector<std::future<std::vector<std::pair<Box, std::string>>>> futures;

    // 根据线程数将任务划分成多个批次进行处理
    for (int i = 0; i < threadCount; ++i) {
        // 计算当前线程需要处理的数据范围索引
        size_t startIdx = i * batchSize;
        // 对于最后一个线程，处理到 geo_objects.size() 为止；其它线程处理 batchSize 个对象
        size_t endIdx = std::min(startIdx + batchSize, geo_objects.size());

        // 确保 endIdx 不大于 geo_objects.size()
        endIdx = std::min(endIdx, geo_objects.size());

        // 记录当前线程处理的索引范围，方便调试
        // logger.log(DEBUG, "线程 " + std::to_string(i) + " 处理索引范围 [" + std::to_string(startIdx) + ", " + std::to_string(endIdx) + ")");

        // 提交任务到线程池，任务的返回值为一个局部数据向量
        futures.push_back(pool.submitTask([this, &geo_objects, startIdx, endIdx]() -> std::vector<std::pair<Box, std::string>> {
            std::vector<std::pair<Box, std::string>> localItems;

            // 局部去重：每个任务内部使用 unordered_set 存储已经处理的 idAndType
            std::unordered_set<std::string> localIds;
            try {
                for (size_t j = startIdx; j < endIdx; ++j) {
                    const auto &obj = geo_objects[j];
                    // 如果没有坐标数据，则跳过
                    if (obj.coordinates.empty()) continue;

                    Box bounding_box;
                    // 根据 GeoObject 的类型计算外接矩形（MBR）
                    if (obj.type == POINT) {
                        bounding_box = Box(obj.coordinates.front(), obj.coordinates.front());
                    } else if (obj.type == LINE) {
                        LineString line(obj.coordinates.begin(), obj.coordinates.end());
                        bounding_box = bg::return_envelope<Box>(line);
                    } else if (obj.type == POLYGON) {
                        Polygon polygon;
                        // 为确保多边形闭合，若首尾不相等，则添加首元素到末尾
                        std::vector<Point> coords = obj.coordinates;
                        if (!bg::equals(coords.front(), coords.back()))
                            coords.push_back(coords.front());
                        polygon.outer().assign(coords.begin(), coords.end());
                        bounding_box = bg::return_envelope<Box>(polygon);
                    } else {
                        continue; // 类型未知则跳过
                    }
                    // 构造用于去重的标识：对象ID和类型的组合字符串
                    std::string idAndType = obj.id + "," + obj.type;
                    if (localIds.find(idAndType) == localIds.end()) {
                        localIds.insert(idAndType);
                        localItems.emplace_back(bounding_box, idAndType);
                    }
                }

            } catch (const std::exception &e) {
                logger.log(WARNING, "构建R树任务异常: " + std::string(e.what()));
            }


            return localItems;
        }));
    }


    // 主线程合并所有局部数据向量
    std::vector<std::pair<Box, std::string>> mergedItems;
    for (auto &fut : futures) {
        auto localVec = fut.get();
        // 直接合并局部数据到 mergedItems
        mergedItems.insert(mergedItems.end(), localVec.begin(), localVec.end());
    }


    // 全局去重：对合并后的数据按照 idAndType 进行排序，然后 unique
    std::sort(mergedItems.begin(), mergedItems.end(), [](const auto &a, const auto &b) {
        return a.second < b.second;
    });
    auto uniqueEnd = std::unique(mergedItems.begin(), mergedItems.end(), [](const auto &a, const auto &b) {
        return a.second == b.second;
    });
    mergedItems.erase(uniqueEnd, mergedItems.end());


    // 将所有数据一次性批量插入到全局 R 树中，减少锁争用
    {
        std::unique_lock<std::shared_mutex> lock(rtree_mutex);
        rtree.insert(mergedItems.begin(), mergedItems.end());
        id = GeoTools::getInitID(fileName);
        logger.log(INFO, "R树构建完成，共插入 " + std::to_string(manager->count()) + " 条数据");
    }


//    // 收集所有线程返回的局部数据，并去重防止重复插入同一ID
//    std::vector<std::pair<Box, std::string>> allData;
//    std::unordered_set<std::string> seenIDs;  // 用于存储已经插入的ID+类型字符串
//
//    for (auto& future : futures) {
//        // 等待每个任务完成并获取局部结果
//        auto batch = future.get();
//        for (const auto& data : batch) {
//            // 如果当前ID没有出现过，则添加到allData中
//            if (seenIDs.insert(data.second).second) {
//                allData.push_back(data);
//            }
//        }
//    }
//
//    // 统一插入到 R 树，减少锁争用
//    {
//        std::unique_lock<std::shared_mutex> lock(rtree_mutex);
//        rtree.insert(allData.begin(), allData.end());
//        logger.log(INFO, "R树构建完成，共插入 " + std::to_string(allData.size()) + " 条数据");
//    }
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



bool RTreeManager::deleteRTree(ThreadPool& pool) {

    // 在共享锁下遍历R树，将所有数据复制到 vector
    std::vector<Value> allItems;
    {
        std::shared_lock<std::shared_mutex> lock(rtree_mutex);
        for (const auto &item : rtree) {
            allItems.push_back(item);
        }
    }

    // 并行查找待删除项：使用线程池对 allItems 进行分段处理
    size_t totalItems = allItems.size();
    // 获取线程池中当前线程数量作为分段数
    int poolThreadCount = pool.getCurrentThreadCount();
    size_t segmentCount = std::min((size_t)poolThreadCount, totalItems);
    if (segmentCount == 0) segmentCount = 1;
    size_t segmentSize = (totalItems + segmentCount - 1) / segmentCount;


    // 定义局部任务返回结构：每个任务返回待删除项 vector<Value>
    std::vector<std::future<std::vector<Value>>> futures;
    for (size_t i = 0; i < segmentCount; ++i) {
        size_t startIdx = i * segmentSize;
        size_t endIdx = std::min(startIdx + segmentSize, totalItems);

        futures.push_back(pool.submitTask([this, &allItems, startIdx, endIdx]() -> std::vector<Value> {
            std::vector<Value> localToRemove;
            for (size_t j = startIdx; j < endIdx; ++j) {
                const auto &item = allItems[j];
                // 解析 item.second 获取 ID（格式 "id,type,..."）
                std::istringstream iss(item.second);
                std::string id_str;
                std::getline(iss, id_str, ',');
                try {
                    unsigned long long obj_id = std::stoull(id_str);
                    // 如果删除集合中包含该 ID，则将该项记录下来
                    if (deleteRtreeSet.find(obj_id) != deleteRtreeSet.end()) {
                        localToRemove.push_back(item);
                    }
                } catch (const std::exception &e) {
                    std::cerr << "警告: 解析 ID 失败 \"" << id_str << "\" - " << e.what() << std::endl;
                }
            }
            return localToRemove;
        }));
    }



    // 3. 合并所有任务返回的待删除项
    std::vector<Value> to_remove;
    for (auto &fut : futures) {
        std::vector<Value> localResult = fut.get();
        to_remove.insert(to_remove.end(), localResult.begin(), localResult.end());
    }

    if (to_remove.empty()) {
        return false; // 无待删除项，直接返回
    }

    // 4. 使用独占锁删除 R 树中的数据
    {
        std::lock_guard<std::shared_mutex> lock(rtree_mutex);
        for (const auto &item : to_remove) {
            rtree.remove(item);
        }
    }

    // 5. 更新删除集合：从 deleteRtreeSet 中移除已删除的 ID
    {
        std::lock_guard<std::mutex> lock(deleteRTree_mutex);
        for (const auto &item : to_remove) {
            std::istringstream ss(item.second);
            std::string id_str;
            std::getline(ss, id_str, ',');
            try {
                unsigned long long obj_id = std::stoull(id_str);
                deleteRtreeSet.erase(obj_id);
            } catch (const std::exception &e) {
                std::cerr << "警告: 删除 ID 时解析失败 \"" << id_str << "\" - " << e.what() << std::endl;
            }
        }
    }

    // 更新全局 id（这里假设 GeoTools::getInitID 获取最新ID）
    id = GeoTools::getInitID(fileName);
    return true;




//    std::vector<Value> to_remove; // 存储待删除项
//
//    {
//        std::shared_lock<std::shared_mutex> lock(rtree_mutex); // 共享锁，允许并发读取 R 树
//
//        // 遍历 R 树，查找匹配的 ID
//        for (const auto& item : rtree) {
//            std::istringstream ss(item.second);
//            std::string id_str;
//            std::getline(ss, id_str, ','); // 获取 ID 字符串
//
//            try {
//                unsigned long long obj_id = std::stoull(id_str);
//                if (deleteRtreeSet.find(obj_id) != deleteRtreeSet.end()) {
//                    to_remove.push_back(item); // 记录待删除项
//                }
//            } catch (const std::exception& e) {
//                std::cerr << "警告: 无法解析 ID \"" << id_str << "\" - " << e.what() << std::endl;
//            }
//        }
//    } // 释放共享锁
//
//    if (to_remove.empty()) {
//        return false; // 无需删除
//    }
//
//    {
//        std::lock_guard<std::shared_mutex> lock(rtree_mutex); // 独占锁，确保安全删除
//        for (const auto& item : to_remove) {
//            rtree.remove(item);
//        }
//    }
//
//    {
//        std::lock_guard<std::mutex> lock(deleteRTree_mutex);
//        for (const auto& item : to_remove) {
//            std::istringstream ss(item.second);
//            std::string id_str;
//            std::getline(ss, id_str, ','); // 获取 ID 字符串
//            try {
//                unsigned long long obj_id = std::stoull(id_str);
//                deleteRtreeSet.erase(obj_id); // 从集合中移除 ID
//            } catch (const std::exception& e) {
//                std::cerr << "警告: 删除 ID 时解析失败 \"" << id_str << "\" - " << e.what() << std::endl;
//            }
//        }
//    }
//
//    id = GeoTools::getInitID(fileName);
//    return true;
}

void RTreeManager::insertBatchRTree(const std::vector<std::pair<Box, std::string>>& batchData) {
    std::unique_lock<std::shared_mutex> lock(rtree_mutex); // 独占锁，防止多个线程并发写入
    rtree.insert(batchData.begin(), batchData.end());
}

std::string RTreeManager::box_to_string(const Box &box) {
    // 获取 box 的最小点和最大点的坐标
    double min_x = bg::get<0>(box.min_corner());
    double min_y = bg::get<1>(box.min_corner());
    double max_x = bg::get<0>(box.max_corner());
    double max_y = bg::get<1>(box.max_corner());

    // 检查是否存在无效数据
    if (std::isnan(min_x) || std::isnan(min_y) ||
        std::isnan(max_x) || std::isnan(max_y)) {
        std::cout << "无效数据" << std::endl;
        return "无效";
    }

    // 创建字符串输出流，并设置固定精度
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6);
    oss << "[(" << min_x << ", " << min_y << "), "
        << "(" << max_x << ", " << max_y << ")]";
    return oss.str();
}





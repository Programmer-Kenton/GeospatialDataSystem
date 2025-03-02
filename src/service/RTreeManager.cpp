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
    // 如果总数不能整除线程数，则每个线程处理的数据数向上取整
    size_t batchSize = (geo_objects.size() + THREAD_INIT_COUNT - 1) / THREAD_INIT_COUNT;  // 等价于向上取整



    logger.log(DEBUG, "每个线程的批次大小: " + std::to_string(batchSize));
    logger.log(INFO, "开始使用线程池构建R树，共 " + std::to_string(THREAD_INIT_COUNT) + " 个线程处理数据");


    // futures 用来保存每个线程返回的局部结果，每个局部结果为一个 vector<pair<Box, string>>
    std::vector<std::future<std::vector<std::pair<Box, std::string>>>> futures;

    // 根据线程数将任务划分成多个批次进行处理
    for (int i = 0; i < THREAD_INIT_COUNT; ++i) {
        // 计算当前线程需要处理的数据范围索引
        size_t startIdx = i * batchSize;
        // 对于最后一个线程，处理到 geo_objects.size() 为止；其它线程处理 batchSize 个对象
        size_t endIdx = std::min(startIdx + batchSize, geo_objects.size());

        // 确保 endIdx 不大于 geo_objects.size()
        endIdx = std::min(endIdx, geo_objects.size());

        // 记录当前线程处理的索引范围，方便调试
        logger.log(DEBUG, "线程 " + std::to_string(i) + " 处理索引范围 [" + std::to_string(startIdx) + ", " + std::to_string(endIdx) + ")");

        // 提交任务到线程池，任务的返回值为一个局部数据向量
        futures.push_back(pool.submitTask([this, &geo_objects, startIdx, endIdx]() -> std::vector<std::pair<Box, std::string>> {
            std::vector<std::pair<Box, std::string>> batchData;
            // 遍历当前线程负责处理的 GeoObject 数据
            for (size_t j = startIdx; j < endIdx; ++j) {
                const auto& obj = geo_objects[j];
                // 如果没有坐标数据，则跳过当前对象
                if (obj.coordinates.empty()) continue;

                Box bounding_box;
                // 根据 GeoObject 的类型，计算外接矩形（MBR）
                if (obj.type == POINT) {
                    // 对于点，其外接矩形就是点本身
                    bounding_box = Box(obj.coordinates.front(), obj.coordinates.front());
                } else if (obj.type == LINE) {
                    // 对于线，构造 LineString 后获取其外接矩形
                    LineString line(obj.coordinates.begin(), obj.coordinates.end());
                    bounding_box = bg::return_envelope<Box>(line);
                } else if (obj.type == POLYGON) {
                    // 对于面，先确保多边形闭合，然后构造 Polygon 并计算外接矩形
                    Polygon polygon;
                    std::vector<Point> mutable_coords = obj.coordinates;
                    if (!bg::equals(mutable_coords.front(), mutable_coords.back())) {
                        mutable_coords.push_back(mutable_coords.front());
                    }
                    polygon.outer().assign(mutable_coords.begin(), mutable_coords.end());
                    bounding_box = bg::return_envelope<Box>(polygon);
                } else {
                    // 如果类型未知，跳过该对象
                    continue;
                }
                // 构造一个字符串，包含对象的ID和类型，用于后续插入R树时使用
                std::string idAndType = obj.id + "," + obj.type;
                batchData.emplace_back(bounding_box, idAndType);
            }
            return batchData;
        }));
    }


    // 收集所有线程返回的局部数据，并去重防止重复插入同一ID
    std::vector<std::pair<Box, std::string>> allData;
    std::unordered_set<std::string> seenIDs;  // 用于存储已经插入的ID+类型字符串

    for (auto& future : futures) {
        // 等待每个任务完成并获取局部结果
        auto batch = future.get();
        for (const auto& data : batch) {
            // 如果当前ID没有出现过，则添加到allData中
            if (seenIDs.insert(data.second).second) {
                allData.push_back(data);
            }
        }
    }

    // 统一插入到 R 树，减少锁争用
    {
        std::unique_lock<std::shared_mutex> lock(rtree_mutex);
        rtree.insert(allData.begin(), allData.end());
        logger.log(INFO, "R树构建完成，共插入 " + std::to_string(allData.size()) + " 条数据");
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

    id = GeoTools::getInitID(fileName);
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





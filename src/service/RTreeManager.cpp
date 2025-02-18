/**
 * @Description TODO
 * @Version 1.0.0
 * @Date 2025/2/11 15:21
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */

#include "RTreeManager.h"
#include "Public.h"


void RTreeManager::buildRTree(const std::vector<GeoObject>& geo_objects) {
    std::lock_guard<std::shared_mutex> lock(rtree_mutex); // 独占锁

    for (const auto& obj : geo_objects) {
        if (obj.coordinates.empty()) continue;
        Box bounding_box;
        if (obj.type == "Point") {
            bounding_box = Box(obj.coordinates.front(), obj.coordinates.front());
        } else if (obj.type == "Line") {
            LineString line(obj.coordinates.begin(), obj.coordinates.end());
            bounding_box = bg::return_envelope<Box>(line);
        } else if (obj.type == "Polygon") {
            Polygon polygon;
            std::vector<Point> mutable_coords = obj.coordinates;
            if (!bg::equals(mutable_coords.front(), mutable_coords.back())) {
                mutable_coords.push_back(mutable_coords.front());
            }
            polygon.outer().assign(mutable_coords.begin(), mutable_coords.end());
            bounding_box = bg::return_envelope<Box>(polygon);
        } else {
            continue;
        }
        std::ostringstream oss;
        oss << obj.id << "," << obj.type;
        rtree.insert(std::make_pair(bounding_box, oss.str()));
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
    rtree.insert(std::make_pair(bounding_box, str));
}



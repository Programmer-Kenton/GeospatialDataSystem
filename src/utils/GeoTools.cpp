/**
 * @Description TODO
 * @Version 1.0.0
 * @Date 2025/2/11 14:57
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */

#include "GeoTools.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>


std::vector<GeoObject> GeoTools::readCSV(const std::string &fileName,size_t startLine,size_t endLine) {
    std::vector<GeoObject> geo_objects;
    std::ifstream file(fileName);

    if (!file.is_open()) {
        std::cerr << "错误: 无法打开文件 - " << fileName << std::endl;
        logger.log(ERROR, "无法打开文件 - " + fileName);  // 记录日志
        return geo_objects;
    }

    std::string line;
    size_t currentLine = 0;

    // 跳过表头
    if (startLine > 0) {
        std::getline(file, line);  // 跳过表头
        currentLine++;
    }

    // 向下跳过startLine之前的行
    while (currentLine < startLine && std::getline(file, line)) {
        currentLine++;
    }

    // 读取数据直到endLine
    while (currentLine < endLine && std::getline(file, line)) {
        if (line.empty()) continue;

        GeoObject obj;
        std::stringstream ss(line);
        std::string id_str, type_str, coords_str;

        // 读取CSV三个字段 ID 类型 坐标数据
        if (!std::getline(ss, id_str, ',') ||
            !std::getline(ss, type_str, ',') ||
            !std::getline(ss, coords_str)) {
            std::cerr << "警告: 行格式错误，已跳过: " << line << std::endl;
            logger.log(WARNING, "行格式错误，已跳过: " + line);  // 记录警告日志
            continue;
        }

        obj.id = id_str;
        obj.type = type_str;

        // 去除坐标字段的额外字符
        coords_str.erase(std::remove(coords_str.begin(), coords_str.end(), '"'), coords_str.end()); // 移除双引号
        coords_str.erase(std::remove(coords_str.begin(), coords_str.end(), '\r'), coords_str.end()); // 移除 \r 符号

        // 解析坐标
        obj.coordinates = GeoTools::parseCoordinates(coords_str);

        if (!obj.coordinates.empty()) {
            geo_objects.push_back(obj);
        } else {
            std::cerr << "警告: 地理对象 " << obj.id << " 坐标解析失败，已跳过。" << std::endl;
            logger.log(WARNING, "地理对象 " + obj.id + " 坐标解析失败，已跳过");  // 记录警告日志
        }

        currentLine++;
    }

    return geo_objects;
}

std::vector<Point> GeoTools::parseCoordinates(const std::string &coord_str) {
    std::vector<Point> coords;
    std::string cleaned_str = coord_str;
    // 移除字符串中反斜杠字符\ 地理信息数据集使用Python在Window平台生成 C++代码在Linux系统可以文件生成有额外转义符
    cleaned_str.erase(std::remove(cleaned_str.begin(),cleaned_str.end(),'\\'),cleaned_str.end());

    // 创建字符流串
    std::stringstream ss(cleaned_str);
    std::string coord_pair;

    // 按空格分割坐标对
    while (std::getline(ss,coord_pair,' ')){
        double x = 0.0,y = 0.0;
        char comma;
        std::stringstream pair_stream(coord_pair);
        pair_stream >> x >> comma >> y;

        // 检查坐标格式是否正确
        if (pair_stream.fail() || comma != ','){
            std::cerr << "解析错误: 坐标格式不正确 \"" << coord_pair << "\"" << std::endl;
            continue;
        }
        coords.emplace_back(x,y);
    }

    return coords;
}

void GeoTools::generateRandomIds(unsigned long long int deleteId, int num) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned long long> dist(1, deleteId);

    // 预分配存储空间 减少动态扩展的开销
    std::unordered_set<unsigned long long> tempSet;

    while (tempSet.size() < static_cast<size_t>(num)){
        tempSet.insert(dist(gen)); // 直接插入 避免重复ID
    }

    {
        std::lock_guard<std::mutex> lock(delete_mutex);
        deleteSet.insert(tempSet.begin(),tempSet.end());
    }

    {
        std::lock_guard<std::mutex> lock(deleteRTree_mutex);
        deleteRtreeSet.insert(tempSet.begin(),tempSet.end());
    }

    // 打印随机生成的ID
    std::cout << "随机生成的ID: ";
    for (const auto& tempId : tempSet) {
        std::cout << tempId << " ";
    }
    std::cout << std::endl;
}

unsigned long long GeoTools::getInitID(const std::string &fileName) {
    // 打开文件并将指针移动到末尾
    std::ifstream file(fileName, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("无法打开文件: " + fileName);
    }

    // 从文件末尾反向查找
    file.seekg(0, std::ios::end);
    std::streampos fileSize = file.tellg();
    if (fileSize == 0) {
        throw std::runtime_error("文件为空");
    }

    std::string last_line;
    char c;
    // 从末尾向前读取字符，直到找到完整的一行
    for (std::streamoff pos = static_cast<std::streamoff>(fileSize) - 1; pos >= 0; --pos) {
        file.seekg(pos);
        file.get(c);
        if (c == '\n' && !last_line.empty()) {
            // 找到换行符且已读取到非空内容
            break;
        }
        last_line.insert(last_line.begin(), c);
    }

    if (last_line.empty()) {
        throw std::runtime_error("文件中无有效的行");
    }

    // 提取最后一行的第一个字段
    std::stringstream ss(last_line);
    std::string id_field;
    std::getline(ss, id_field, ','); // 按逗号分割获取第一个字段

    // 转换为整数类型
    try {
        return std::stoull(id_field); // 使用 std::stoull 转换为 unsigned long long
    } catch (const std::exception &e) {
        throw std::runtime_error("ID 解析错误: " + std::string(e.what()));
    }
}

void GeoTools::modifyCSV(const std::string &fileName) {
    // 每次处理十万行数据
    const size_t bufferSize = 100000;

    bool didInsert = false,didDelete = false;

    // 插入数据
    {
        std::lock_guard<std::mutex> lock(insert_mutex);
        if (!insertVec.empty()) {
            std::ofstream file(fileName, std::ios::app);
            if (!file.is_open()) {
                std::cerr << "错误: 无法打开文件 - " << fileName << std::endl;
            } else {
                std::cout << "写入前 insertVec的尺寸: " << insertVec.size() << std::endl;

                for (auto it = insertVec.begin(); it != insertVec.end();) {
                    std::ostringstream oss;
                    oss << it->id << "," << it->type << ",\"";
                    for (size_t i = 0; i < it->coordinates.size(); ++i) {
                        oss << std::fixed << std::setprecision(6)
                            << it->coordinates[i].get<0>() << ","
                            << it->coordinates[i].get<1>();
                        if (i != it->coordinates.size() - 1) oss << " ";
                    }
                    oss << "\"";
                    file << oss.str() << std::endl;

                    it = insertVec.erase(it);
                }
                file.close();
                didInsert = true;
                std::cout << "数据插入完成。" << std::endl;
            }
        } else {
            std::cout << "无插入数据，跳过插入部分。" << std::endl;
        }
    }


    // 删除数据
    {
        std::lock_guard<std::mutex> lock(delete_mutex);
        if (!deleteSet.empty()) {
            std::ifstream inputFile(fileName);
            if (!inputFile.is_open()) {
                std::cerr << "错误: 无法打开文件 - " << fileName << std::endl;
                return;
            }

            std::ofstream tempFile(fileName + ".tmp");
            if (!tempFile.is_open()) {
                std::cerr << "错误: 无法创建临时文件 - " << fileName + ".tmp" << std::endl;
                inputFile.close();
                return;
            }

            std::string header;
            if (std::getline(inputFile, header)) {
                tempFile << header << '\n';  // 直接写入表头
            }

            std::string line;
            while (std::getline(inputFile, line)) {
                if (line.empty()) continue;

                std::istringstream ss(line);
                std::string idStr;
                if (!std::getline(ss, idStr, ',')) continue;

                try {
                    unsigned long long lineID = std::stoull(idStr);
                    if (deleteSet.find(lineID) != deleteSet.end()) {
                        std::cout << "删除 CSV 行: " << line << std::endl;
                        continue;  // 跳过该行
                    }
                } catch (const std::exception &e) {
                    std::cerr << "警告: 无法解析行 ID \"" << idStr << "\" - " << e.what() << std::endl;
                }

                tempFile << line << '\n';
            }

            inputFile.close();
            tempFile.close();

            // 替换原始文件
            if (std::remove(fileName.c_str()) != 0) {
                std::cerr << "错误: 无法删除原始文件 - " << fileName << std::endl;
                return;
            }
            if (std::rename((fileName + ".tmp").c_str(), fileName.c_str()) != 0) {
                std::cerr << "错误: 无法重命名临时文件为原始文件 - " << fileName << std::endl;
                return;
            }

            // 清空 deleteSet
            deleteSet.clear();
            didDelete = true;
            std::cout << "删除任务完成，deleteSet 已清空。" << std::endl;
        } else {
            std::cout << "无删除数据，跳过删除部分。" << std::endl;
        }
    }


    if (!didInsert && !didDelete) {
        std::cout << "无插入数据，也无删除数据，modifyCSV() 无需执行。" << std::endl;
    }

    std::cout << "GeoTools::modifyCSV() 执行结束。" << std::endl;
}

size_t GeoTools::countLines(const std::string &fileName) {
    std::ifstream file(fileName, std::ios::in);
    if (!file.is_open()) {
        std::cerr << "错误: 无法打开文件 - " << fileName << std::endl;
        return 0;
    }

    size_t lineCount = 0;
    std::string line;
    while (std::getline(file, line)) {
        lineCount++;
    }

    return lineCount;
}






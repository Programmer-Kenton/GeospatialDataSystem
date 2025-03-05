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

    // 打开文件 只读方式
    int fd = open(fileName.c_str(),O_RDONLY);
    if (fd == -1){
        std::cerr << "错误: 无法打开文件 - " << fileName << std::endl;
        logger.log(ERROR,"无法打开文件 - " + fileName);
        return geo_objects;
    }

    // 通过fstat获取文件大小
    struct stat st;
    if (fstat(fd,&st) == -1){
        std::cerr << "错误: fstat 失败 - " << fileName << std::endl;
        logger.log(ERROR, "fstat 失败 - " + fileName);
        close(fd);
        return geo_objects;
    }

    size_t fileSize = st.st_size;
    if (fileSize == 0){
        std::cerr << "错误: 文件为空 - " << fileName << std::endl;
        logger.log(WARNING, "文件为空 - " + fileName);
        close(fd);
        return geo_objects;
    }


    // 使用mmap将整个文件映射到内存 使用MAP_PRIVATE模式 只读
    void* mapped = mmap(nullptr,fileSize,PROT_READ,MAP_PRIVATE,fd,0);
    if (mapped == MAP_FAILED){
        std::cerr << "错误: mmap 失败 - " << fileName << std::endl;
        logger.log(ERROR, "mmap 失败 - " + fileName);
        close(fd);
        return geo_objects;
    }


    // 文件映射成功后，可关闭文件描述符
    close(fd);

    // 将映射区转换为 char* 指针，便于逐字符处理
    char* data = static_cast<char*>(mapped);
    char* dataEnd = data + fileSize;

    size_t currentLine = 0;  // 当前行号
    char* lineStart = data;  // 当前行的起始地址



    // 遍历整个映射区，按换行符分割出各行
    while (lineStart < dataEnd) {
        // 查找当前行中的换行符 '\n'
        char* lineEnd = static_cast<char*>(memchr(lineStart, '\n', dataEnd - lineStart));
        if (!lineEnd) {
            // 如果找不到换行符，则认为剩余部分为一行
            lineEnd = dataEnd;
        }
        currentLine++; // 行号递增

        // 仅解析在指定行范围内的行（startLine 到 endLine, 包含两端）
        if (currentLine >= startLine && currentLine <= endLine) {
            std::string line(lineStart, lineEnd - lineStart);
            // 去除 Windows 平台可能存在的 '\r' 字符
            line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
            if (!line.empty()) {
                std::stringstream ss(line);
                std::string id_str, type_str, coords_str;
                // 解析 CSV 字段：假设 CSV 格式为 id,type,coords
                if (!std::getline(ss, id_str, ',') ||
                    !std::getline(ss, type_str, ',') ||
                    !std::getline(ss, coords_str)) {
                    std::cerr << "警告: 行格式错误，已跳过: " << line << std::endl;
                    logger.log(WARNING, "行格式错误，已跳过: " + line);
                } else {
                    GeoObject obj;
                    obj.id = id_str;
                    obj.type = type_str;
                    // 去除多余字符（例如双引号）
                    coords_str.erase(std::remove(coords_str.begin(), coords_str.end(), '"'), coords_str.end());
                    // 解析坐标字符串，生成坐标向量（parseCoordinates 函数需自行实现）
                    obj.coordinates = GeoTools::parseCoordinates(coords_str);
                    if (!obj.coordinates.empty()) {
                        geo_objects.push_back(obj);
                    } else {
                        std::cerr << "警告: 地理对象 " << obj.id << " 坐标解析失败，已跳过。" << std::endl;
                        logger.log(WARNING, "地理对象 " + obj.id + " 坐标解析失败，已跳过");
                    }
                }
            }
        }
        // 移动到下一行（当前行末尾的下一个字符）
        lineStart = lineEnd + 1;
        // 如果当前行号已经达到或超过 endLine，则退出循环
        if (currentLine >= endLine) break;
    }

    // 解除内存映射，释放资源
    munmap(mapped, fileSize);
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

void GeoTools::generateRandomIds(int num) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned long long> dist(1, id);

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

    // 获取实际要删除的数量
    deleteCount = deleteRtreeSet.size();

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
                logger.log(ERROR,"错误: 无法打开文件 - " + fileName);
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
                logger.log(ERROR,"错误: 无法打开文件 - " + fileName);
                return;
            }

            std::ofstream tempFile(fileName + ".tmp");
            if (!tempFile.is_open()) {
                std::cerr << "错误: 无法创建临时文件 - " << fileName + ".tmp" << std::endl;
                logger.log(ERROR,"错误: 无法创建临时文件 - " + fileName + ".tmp");
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
                    logger.log(WARNING,"无法解析行 ID" + idStr + " - " + e.what());
                }

                tempFile << line << '\n';
            }

            inputFile.close();
            tempFile.close();

            // 替换原始文件
            if (std::remove(fileName.c_str()) != 0) {
                std::cerr << "错误: 无法删除原始文件 - " << fileName << std::endl;
                logger.log(ERROR,"错误: 无法删除原始文件 - " + fileName);
                return;
            }
            if (std::rename((fileName + ".tmp").c_str(), fileName.c_str()) != 0) {
                std::cerr << "错误: 无法重命名临时文件为原始文件 - " << fileName << std::endl;
                logger.log(ERROR,"错误: 无法重命名临时文件为原始文件 - " + fileName);
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






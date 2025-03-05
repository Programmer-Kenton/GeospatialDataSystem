/**
 * @Description 工具类
 * @Version 1.0.0
 * @Date 2025/2/11 14:57
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */
#ifndef GEOSPATIALDATASYSTEM_GEOTOOLS_H
#define GEOSPATIALDATASYSTEM_GEOTOOLS_H

#include "Public.h"
#include <vector>
#include <string>
#include <exception>
#include <iostream>
#include <sstream>
#include <random>
#include <unordered_set>

class GeoTools {
public:
    // 读取CSV文件数据放入R树
    // 优化代码采取mmap读取块
    static std::vector<GeoObject> readCSV(const std::string &fileName,size_t startLine,size_t endLine);

    // 统计CSV文件行数
    static size_t countLines(const std::string &fileName);

    // 解析坐标字符串
    static std::vector<Point> parseCoordinates(const std::string &coord_str);

    // 生成num个不超过id的随机ID值 模拟传入要删除的id
    static void generateRandomIds(int num);

    // 生成初始ID号
    static unsigned long long getInitID(const std::string &fileName);

    // 修改CSV文件中的数据
    static void modifyCSV(const std::string& fileName);


};


#endif //GEOSPATIALDATASYSTEM_GEOTOOLS_H

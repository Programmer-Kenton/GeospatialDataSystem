/**
 * @Description 测试单线程构建R树耗时
 * @Version 1.0.0
 * @Date 2025/2/18 17:02
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */

//#include "GeoServer.h"
//
//GeoServer *pServer = new GeoServer;

#include "Public.h"
#include "RTreeManager.h"

bgi::rtree<Value, bgi::quadratic<64>> rtree; // R树实例
std::shared_mutex rtree_mutex; // R树线程安全共享锁

// 解析坐标字符串
std::vector<Point> parseCoordinates(const std::string &coord_str) {

    std::vector<Point> coords;
    std::string cleaned_str = coord_str;
    // 移除字符串中反斜杠字符\ 地理信息数据集使用Python在Window平台生成 C++代码在Linux系统可以文件生成有额外转义符
    cleaned_str.erase(std::remove(cleaned_str.begin(), cleaned_str.end(), '\\'), cleaned_str.end());

    // 创建字符流串
    std::stringstream ss(cleaned_str);
    std::string coord_pair;

    // 按空格分割坐标对
    while (std::getline(ss, coord_pair, ' ')) {
        double x = 0.0, y = 0.0;
        char comma;
        std::stringstream pair_stream(coord_pair);
        pair_stream >> x >> comma >> y;

        // 检查坐标格式是否正确
        if (pair_stream.fail() || comma != ',') {
            std::cerr << "解析错误: 坐标格式不正确 \"" << coord_pair << "\"" << std::endl;
            continue;
        }
        coords.emplace_back(x, y);
    }

    return coords;
}

// 单线程读取CSV文件
std::vector<GeoObject> readCSV(std::string &fileName) {
    std::vector<GeoObject> geo_objects;
    std::ifstream file(fileName);

    if (!file.is_open()) {
        std::cerr << "错误: 无法打开文件 - " << fileName << std::endl;
        return geo_objects;
    }

    std::string line;
    std::getline(file, line); // 跳过表头

    while (std::getline(file, line)) {

        if (line.empty()) continue; // 跳过空行

        std::stringstream ss(line);
        std::string id_str, type_str, coords_str;

        // 检查字段数量是否正确
        if (!std::getline(ss, id_str, ',') ||
            !std::getline(ss, type_str, ',') ||
            !std::getline(ss, coords_str)) {
            std::cerr << "警告: 行格式不正确 \"" << line << "\", 已跳过。" << std::endl;
            continue;
        }

        // 创建 GeoObject 实例
        GeoObject obj;

        // 解析 ID
        try {
            obj.id = std::stoull(id_str);
        } catch (const std::exception &e) {
            std::cerr << "警告: ID \"" << id_str << "\" 转换失败 - " << e.what() << std::endl;
            continue;
        }

        // 解析类型
        obj.type = type_str;

        // 移除双引号和回车符
        coords_str.erase(std::remove(coords_str.begin(), coords_str.end(), '"'), coords_str.end());
        coords_str.erase(std::remove(coords_str.begin(), coords_str.end(), '\r'), coords_str.end());

        // 解析坐标
        obj.coordinates = parseCoordinates(coords_str);

        // 地理对象初始状态为0
        // obj.status = 0;

        if (!obj.coordinates.empty()) {
            geo_objects.push_back(obj);
        } else {
            std::cerr << "警告: 地理对象 " << obj.id << " 坐标解析失败，已跳过。" << std::endl;
        }
    }

    return geo_objects;
}

// 单线程构建R树
void buildRTree(const std::vector<GeoObject> &geo_objects) {

    std::lock_guard<std::shared_mutex> lock(rtree_mutex); // 独占锁

    for (const auto &obj: geo_objects) {
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


int main() {
    // 读取 CSV 文件数据
    std::cout << "程序开始... 单线程读取CSV文件地理信息数据" << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    auto geo_objects = readCSV(fileName);
    if (geo_objects.empty()) {
        std::cerr << "错误: 没有读取到任何地理对象。" << std::endl;
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    std::cout << "读取CSV文件数据结束耗时: " << duration.count() << "秒" << std::endl;

    // 构建R树索引
    std::cout << "程序开始... 计算每个地理信息的最小外接框并构建R树索引(二次分裂算法构建):" << std::endl;
    auto start2 = std::chrono::high_resolution_clock::now();

    buildRTree(geo_objects);

    auto end2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration2 = end2 - start2;
    std::cout << "二次分裂算法 构建R树耗时: " << duration2.count() << "秒" << std::endl;


    // 打印R树内容
//    std::cout << "开始打印R树内容..." << std::endl;
//    manager->printRTree();
    // 查询面坐标
    std::cout << "请输入面坐标 (经度,纬度 经度,纬度 ...): ";
    std::string input;
    std::getline(std::cin, input);

    auto coords = parseCoordinates(input);
    if (coords.size() < 3) {
        std::cerr << "错误: 面查询至少需要三个坐标点。" << std::endl;
        return 1;
    }

    if (!bg::equals(coords.front(), coords.back())) {
        coords.push_back(coords.front()); // 确保闭合
    }

    Polygon query_polygon;
    query_polygon.outer().assign(coords.begin(), coords.end());
    Box query_box = bg::return_envelope<Box>(query_polygon);

    std::cout << "查询开始..." << std::endl;
    auto query_start = std::chrono::high_resolution_clock::now();

    // 查询R树
    std::vector<Value> result;
    rtree.query(bgi::intersects(query_box), std::back_inserter(result));

    auto query_end = std::chrono::high_resolution_clock::now();


    // 统计结果
    int point_count = 0, line_count = 0, polygon_count = 0;
    for (const auto &item : result) {
        std::istringstream iss(item.second);
        std::string id, type;
        std::getline(iss, id, ',');
        std::getline(iss, type, ',');

        if (type == "Point") point_count++;
        else if (type == "Line") line_count++;
        else if (type == "Polygon") polygon_count++;

        std::cout << "匹配对象: ID=" << id << ", 类型=" << type << std::endl;
    }

    std::chrono::duration<double> query_duration = query_end - query_start;
    std::cout << "查询结束 耗时: " << query_duration.count() << "秒" << std::endl;

    // 打印统计信息
    std::cout << "\n查询结果统计:" << std::endl;
    std::cout << "点的个数: " << point_count << std::endl;
    std::cout << "线的个数: " << line_count << std::endl;
    std::cout << "面的个数: " << polygon_count << std::endl;

    return 0;


}
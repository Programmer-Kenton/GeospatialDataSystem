/**
 * @Description TODO
 * @Version 1.0.0
 * @Date 2025/2/11 14:33
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */

#include "GeoServer.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <chrono>
#include <iostream>

using json = nlohmann::json;
using namespace std::chrono;

GeoServer *pServer = new GeoServer();

// 处理查询请求
void handleQuery(const httplib::Request &req, httplib::Response &res) {
    try {
        // 解析 JSON 请求体
        json request_data;
        try {
            request_data = json::parse(req.body);
        } catch (const json::exception &e) {
            res.status = 400;
            res.set_content("JSON 解析失败: " + std::string(e.what()), "text/plain");
            return;
        }

        // 检查是否有 coordinates 字段，并且它是一个数组
        if (!request_data.contains("coordinates") || !request_data["coordinates"].is_array()) {
            res.status = 400;
            res.set_content("错误: 缺少 'coordinates' 字段，或者格式错误", "text/plain");
            return;
        }

        // 解析坐标数组
        std::vector<Point> coords;
        for (const auto &coord : request_data["coordinates"]) {
            if (!coord.is_array() || coord.size() != 2) {
                res.status = 400;
                res.set_content("错误: 坐标格式错误，应为 [[lng, lat], [lng, lat], ...]", "text/plain");
                return;
            }
            // 获取经纬度坐标
            double lng = coord[0].get<double>();
            double lat = coord[1].get<double>();
            coords.emplace_back(lng, lat);
        }

        // 面查询至少需要 3 个坐标点
        if (coords.size() < 3) {
            res.status = 400;
            res.set_content("错误: 面查询至少需要三个坐标点。", "text/plain");
            return;
        }

        // 确保闭合坐标
        if (!bg::equals(coords.front(), coords.back())) {
            coords.push_back(coords.front());
        }

        // 创建查询多边形，并计算最小外接矩形（MBR）
        Polygon query_polygon;
        query_polygon.outer().assign(coords.begin(), coords.end());
        Box query_box = bg::return_envelope<Box>(query_polygon);

        std::cout << "查询开始..." << std::endl;
        auto query_start = high_resolution_clock::now();
        std::vector<Value> result = manager->searchRTree(query_box);
        if (result.empty()) {
            std::cerr << "查询结果为空" << std::endl;
            logger.log(DEBUG, "查询结果为空");
        }
        auto query_end = high_resolution_clock::now();
        duration<double> query_duration = query_end - query_start;
        logger.log(INFO, "查找符合条件的地理信息数据耗时:" + std::to_string(query_duration.count()));
        std::cout << "查询结束 耗时: " << query_duration.count() << "秒" << std::endl;

        // 构造返回的 JSON 对象（必须初始化为对象）
        json response_data = json::object();
        // 用于保存查询结果的数组
        json data_array = json::array();

        int point_count = 0, line_count = 0, polygon_count = 0;
        for (const auto &item : result) {
            std::istringstream iss(item.second);
            std::string id, type;
            std::getline(iss, id, ',');
            std::getline(iss, type, ',');

            if (type == POINT)
                point_count++;
            else if (type == LINE)
                line_count++;
            else if (type == POLYGON)
                polygon_count++;

            // 使用 manager->box_to_string 将 Box 转换为字符串
            std::string coordinates_str = manager->box_to_string(item.first);
            std::cout << "id = " << id << " type = " << type << " coordinates = " << coordinates_str << std::endl;

            // 创建一个 JSON 对象表示单个查询结果
            json result_item;
            result_item["id"] = id;
            result_item["type"] = type;
            result_item["coordinates"] = coordinates_str;
            data_array.push_back(result_item);
        }

        // 设置统计信息和查询结果
        response_data["statistics"] = {
                {"point_count", point_count},
                {"line_count", line_count},
                {"polygon_count", polygon_count}
        };
        response_data["data"] = data_array;

        // 设置响应状态码为 200，并返回响应内容
        res.status = 200;
        logger.log(INFO, "准备设置响应内容: " + response_data.dump(4));
        res.set_content(response_data.dump(), "application/json");
        std::cout << "返回前端数据，状态码: " << res.status << std::endl;
        std::cout << "debug: function reached here!" << std::endl;
    } catch (const std::exception &e) {
        logger.log(ERROR, "服务器内部错误: " + std::string(e.what()));
        res.status = 500;
        res.set_content("服务器内部错误: " + std::string(e.what()), "text/plain");
        std::cout << "服务器内部错误: " << e.what() << std::endl;
    }
}

int main(int argc, char *argv[]) {
    // 项目初始化
    pServer->initSystem();

    // 设置默认 CORS 头
    server.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"}
    });

    // 处理 OPTIONS 预检请求
    server.Options("/query", [](const httplib::Request &, httplib::Response &res) {
        res.status = 200;
    });

    // 接收前端查询地理信息数据请求
    server.Post("/query", handleQuery);

    // 绑定8080端口并开始监听
    server.listen("0.0.0.0", 8080);

    getchar();
}

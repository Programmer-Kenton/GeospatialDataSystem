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

GeoServer *pServer = new GeoServer();

// 设置 CORS 头
void setCORSHeaders(httplib::Response &res, const std::string &origin) {
    res.set_header("Access-Control-Allow-Origin", origin); // 动态设置允许访问的源
    res.set_header("Access-Control-Allow-Methods", "POST, DELETE, PUT, GET, OPTIONS");  // 允许的请求方法
    res.set_header("Access-Control-Allow-Headers", "Content-Type");  // 允许的请求头
    res.set_header("Access-Control-Allow-Credentials", "true");  // 支持 Cookie
}

// 处理查询请求
void handleQuery(const httplib::Request &req, httplib::Response &res) {
    try {

        // 动态设置 CORS 头
        setCORSHeaders(res, req.get_header_value("Origin"));

        // 解析 JSON 请求体
        nlohmann::json request_data;
        try {
            request_data = nlohmann::json::parse(req.body);
        } catch (const nlohmann::json::exception &e) {
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
        auto query_start = std::chrono::high_resolution_clock::now();
        std::vector<Value> result = manager->searchRTree(query_box);
        if (result.empty()) {
            std::cerr << "查询结果为空" << std::endl;
            logger.log(DEBUG, "查询结果为空");
        }
        auto query_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> query_duration = query_end - query_start;
        logger.log(INFO, "查找符合条件的地理信息数据耗时:" + std::to_string(query_duration.count()));
        std::cout << "查询结束 耗时: " << query_duration.count() << "秒" << std::endl;

        // 构造返回的 JSON 对象（必须初始化为对象）
        nlohmann::json response_data = nlohmann::json::object();
        // 用于保存查询结果的数组
        nlohmann::json data_array = nlohmann::json::array();

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
            // std::cout << "id = " << id << " type = " << type << " coordinates = " << coordinates_str << std::endl;

            // 创建一个 JSON 对象表示单个查询结果
            nlohmann::json result_item;
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
    } catch (const std::exception &e) {
        logger.log(ERROR, "服务器内部错误: " + std::string(e.what()));
        setCORSHeaders(res, req.get_header_value("Origin")); // 异常时补设
        res.status = 500;
        res.set_content("服务器内部错误: " + std::string(e.what()), "text/plain");
        std::cout << "服务器内部错误: " << e.what() << std::endl;
    }
}

// 删除单个删除请求
void handleDelete(const httplib::Request &req,httplib::Response &res){
    try{

        // 动态设置 CORS 头
        setCORSHeaders(res, req.get_header_value("Origin"));

        // 从URL正则匹配结果中获取id
        int deleteId = std::stoi(req.matches[1]);

        // 默认删除数量为1 如果请求体中有指定则使用请求体中的数量
        int num = 1;

        // 调用后端删除接口
        bool success = pServer->deleteRtreeData(id,1,deleteId);

        if (success) {
            res.status = 200;
            res.set_content("删除成功", "text/plain");
            logger.log(DEBUG,"删除前端传来的单个id数据后R树的尺寸为: " + std::to_string(manager->count()));
        } else {
            res.status = 404;
            res.set_content("数据不存在", "text/plain");
        }

    }catch(const std::exception &e){
        setCORSHeaders(res, req.get_header_value("Origin")); // 异常时补设
        res.status = 500;
        res.set_content("删除失败: " + std::string(e.what()), "text/plain");
    }
}

// TODO 用户传入随机要删除的数据条数
void handleInsertGeoData(const httplib::Request &req, httplib::Response &res){
    try {

        // 动态设置 CORS 头
        setCORSHeaders(res, req.get_header_value("Origin"));

        // 解析请求体中的 JSON 数据
        nlohmann::json request_data;
        try {
            request_data = nlohmann::json::parse(req.body);
        } catch (const nlohmann::json::exception &e) {
            res.status = 400;
            res.set_content("JSON 解析失败: " + std::string(e.what()), "text/plain");
            return;
        }

        // 检查是否有 num 字段
        if (!request_data.contains("num") || !request_data["num"].is_number()) {
            res.status = 400;
            res.set_content("错误: 缺少 'num' 字段或 'num' 不是数字", "text/plain");
            return;
        }

        // 获取随机生成的数量
        int num = request_data["num"].get<int>();

        // 检查生成数量是否在合理范围（1万到10万之间）
        if (num < 10000 || num > 100000) {
            res.status = 400;
            res.set_content("错误: 生成数量必须在10000到100000之间", "text/plain");
            return;
        }

        bool success = pServer->insertRTreeData(num,id);

        if (success){
            // 返回成功响应
            res.status = 200;
            res.set_content("成功生成并插入 " + std::to_string(num) + " 条数据", "text/plain");
            logger.log(DEBUG,"成功生成并插入 " + std::to_string(num) + " 条数据");
        } else{
            res.status = 500;
            res.set_content("插入数据失败", "text/plain");
            logger.log(DEBUG,"插入数据失败");
        }

    } catch (const std::exception &e) {
        res.status = 500;
        res.set_content("插入失败: " + std::string(e.what()), "text/plain");
    }
}

// 处理 OPTIONS 请求
void handleOptions(const httplib::Request &req, httplib::Response &res) {
    setCORSHeaders(res, req.get_header_value("Origin"));

    // 根据路径设置允许的方法
    if (req.path == "/query") {
        res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
    } else if (req.path.find("/delete/") != std::string::npos) {
        res.set_header("Access-Control-Allow-Methods", "DELETE, OPTIONS");
    } else if (req.path == "/insert") {
        res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
    }

    res.status = 204; // 使用 204 No Content 状态码
}


// 处理查询和删除等请求的主逻辑
void setupServer() {

    // 处理 OPTIONS 请求
    server.Options("/query", handleOptions);
    server.Options("/delete/(\\d+)", handleOptions);
    server.Options("/insert", handleOptions);

    // 绑定路由
    server.Post("/query", handleQuery);
    server.Delete(R"(/delete/(\d+))", handleDelete);
    server.Post("/insert", handleInsertGeoData);
}

// 备份代码
int main(int argc, char *argv[]) {
    // 项目初始化
    pServer->initSystem();

    // 设置并启动服务
    setupServer();

    // 启动监听 8080 端口
    server.listen("0.0.0.0", 8080);

    getchar();
}

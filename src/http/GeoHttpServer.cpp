/**
 * @Description 处理前端业务请求
 * @Version 1.0.0
 * @Date 2025/3/2 16:55
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */

#include "GeoHttpServer.h"


GeoHttpServer::GeoHttpServer(GeoServer *server) :server(server){

}

void GeoHttpServer::handleQuery(const httplib::Request &req, httplib::Response &res) {
    try {
        // 解析 JSON 请求体
        nlohmann::json request_data;
        try {
            request_data = nlohmann::json::parse(req.body);
        } catch (const nlohmann::json::exception &e) {
            res.status = 400;
            res.set_content(R"({"status": "error", "message": "JSON 解析失败"})", "application/json");
            logger.log(WARNING,R"({"status": "error", "message": "JSON 解析失败"})");
            return;
        }

        // 检查坐标
        if (!request_data.contains("coordinates") || !request_data["coordinates"].is_array()) {
            res.status = 400;
            res.set_content(R"({"status": "error", "message": "错误: 坐标数据缺失或格式错误"})", "application/json");
            logger.log(WARNING,R"({"status": "error", "message": "错误: 坐标数据缺失或格式错误"})");
            return;
        }

        // 解析坐标
        std::vector<Point> coords;
        for (const auto &coord : request_data["coordinates"]) {
            if (!coord.is_array() || coord.size() != 2) {
                res.status = 400;
                res.set_content(R"({"status": "error", "message": "错误: 坐标格式错误，应为 [[lng, lat], [lng, lat], ...]"})", "application/json");
                logger.log(WARNING,R"({"status": "error", "message": "错误: 坐标格式错误，应为 [[lng, lat], [lng, lat], ...]"})");
                return;
            }
            coords.emplace_back(coord[0].get<double>(), coord[1].get<double>());
        }

        // 调用查询函数
        nlohmann::json response_data = server->queryRTreePolygon(coords);

        // 处理响应
        res.status = (response_data["status"] == "success") ? 200 : 400;
        res.set_content(response_data.dump(), "application/json");

    } catch (const std::exception &e) {
        res.status = 500;
        res.set_content(R"({"status": "error", "message": "服务器内部错误"})", "application/json");
        logger.log(ERROR,R"({"status": "error", "message": "服务器内部错误"})");
    }
}

void GeoHttpServer::handleDelete(const httplib::Request &req, httplib::Response &res) {
    try {

        int deleteId = std::stoi(req.matches[1]);
        int num = 1;

        auto start = std::chrono::high_resolution_clock::now();
        bool success = server->deleteRtreeData(num, deleteId);
        res.status = success ? 200 : 404;
        res.set_content(success ? "删除成功" : "数据不存在", "application/plain");

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        if (success){
            logger.log(INFO,"删除ID: " + std::to_string(deleteId) + "成功... 耗时: " + std::to_string(duration.count()) + "秒");
        } else{
            logger.log(ERROR,"删除ID: " + std::to_string(deleteId) + "失败...");
        }

    } catch (const std::exception &e) {
        res.status = 500;
        res.set_content("删除失败: " + std::string(e.what()), "application/plain");
        logger.log(WARNING,"删除失败: " + std::string(e.what()));
    }
}

void GeoHttpServer::handleInsertGeoData(const httplib::Request &req, httplib::Response &res) {
    try {
        // 解析请求体
        nlohmann::json request_data;
        try {
            request_data = nlohmann::json::parse(req.body);
        } catch (const nlohmann::json::exception &e) {
            res.status = 400;
            res.set_content(R"({"status":"error","message":"JSON 解析失败"})", "application/json");
            return;
        }

        // 检查是否包含 num 字段
        if (!request_data.contains("num") || !request_data["num"].is_number()) {
            res.status = 400;
            res.set_content(R"({"status":"error","message":"缺少 'num' 字段或格式错误"})", "application/json");
            return;
        }

        // 获取插入数据的数量
        int num = request_data["num"].get<int>();
        if (num < 10000 || num > 100000) {
            res.status = 400;
            res.set_content(R"({"status":"error","message":"插入数量必须在 10000-100000 之间"})", "application/json");
            return;
        }

        bool success = server->insertRTreeData(num, id);
        if (success) {
            nlohmann::json response_data;
            response_data["status"] = "success";
            response_data["message"] = "数据插入成功";
            res.status = 200;
            res.set_content(response_data.dump(), "application/json");
        } else {
            res.status = 500;
            res.set_content(R"({"status":"error","message":"数据插入失败"})", "application/json");
        }
    } catch (const std::exception &e) {
        res.status = 500;
        res.set_content(R"({"status":"error","message":"服务器内部错误"})", "application/json");
    }
}

void GeoHttpServer::handleDeleteGeoData(const httplib::Request &req, httplib::Response &res) {
    try {

        // 解析请求体中的JSON数据
        nlohmann::json request_data;
        try{
            request_data = nlohmann::json::parse(req.body);
        }catch (const nlohmann::json::exception &e){
            res.status = 400;
            res.set_content("JSON 解析失败: " + std::string(e.what()), "application/plain");
            return;
        }

        // 检查是否有num字段
        if (!request_data.contains("num") || !request_data["num"].is_number()){
            res.status = 400;
            res.set_content("错误: 缺少 'num' 字段或 'num' 不是数字", "application/plain");
            return;
        }

        // 获取随机删除的数量
        int num = request_data["num"].get<int>();

        // 检查删除数量是否在合理范围 这里的num不能大于全局变量id
        id = GeoTools::getInitID(fileName);
        if (num < 1 || num > id){
            res.status = 400;
            res.set_content("错误: 删除数量必须小于" + std::to_string(id), "application/plain");
            return;
        }

        // 调用后端删除接口
        bool success = server->deleteRtreeData(num);

        if (success){
            // 返回成功响应
            nlohmann::json response_data;
            response_data["status"] = "success";
            response_data["deleted_count"] = num;
            response_data["message"] = "可能随机生成重复ID值,实际成功删除 " + std::to_string(deleteCount) + " 条数据";
            deleteCount = -1;
            res.status = 200;
            res.set_content(response_data.dump(), "application/json");
        } else{
            nlohmann::json response_data;
            response_data["status"] = "error";
            response_data["error"] = "删除数据失败";
            res.status = 500;
            res.set_content(response_data.dump(), "application/json");
        }

    } catch (const std::exception &e) {
        res.status = 500;
        res.set_content("删除失败: " + std::string(e.what()), "application/plain");
    }
}

void GeoHttpServer::handleOptions(const httplib::Request &req, httplib::Response &res) {
    setCORSHeaders(res, req.get_header_value("Origin"));
    res.status = 204;
}

void GeoHttpServer::setupRoutes(httplib::Server &server) {
    // 处理 OPTIONS 请求
    server.Options("/query", [this](const httplib::Request &req, httplib::Response &res) { handleOptions(req, res); });
    server.Options("/delete/(\\d+)", [this](const httplib::Request &req, httplib::Response &res) { handleOptions(req, res); });
    server.Options("/insert", [this](const httplib::Request &req, httplib::Response &res) { handleOptions(req, res); });
    server.Options("/delete-random", [this](const httplib::Request &req, httplib::Response &res) { handleOptions(req, res); });
    server.Options("/count", [this](const auto& req, auto& res) { handleOptions(req, res); });

    // 处理业务请求
    server.Post("/query", [this](const httplib::Request &req, httplib::Response &res) {
        setCORSHeaders(res, req.get_header_value("Origin")); // 统一设置 CORS 头
        handleQuery(req, res);
    });

    server.Delete(R"(/delete/(\d+))", [this](const httplib::Request &req, httplib::Response &res) {
        setCORSHeaders(res, req.get_header_value("Origin")); // 统一设置 CORS 头
        handleDelete(req, res);
    });

    server.Post("/insert", [this](const httplib::Request &req, httplib::Response &res) {
        setCORSHeaders(res, req.get_header_value("Origin")); // 统一设置 CORS 头
        handleInsertGeoData(req, res);
    });

    server.Post("/delete-random", [this](const httplib::Request &req, httplib::Response &res) {
        setCORSHeaders(res, req.get_header_value("Origin")); // 统一设置 CORS 头
        handleDeleteGeoData(req, res);
    });

    server.Get("/count", [this](const httplib::Request &req, httplib::Response &res) {
        setCORSHeaders(res, req.get_header_value("Origin"));
        handleGetTotal(req, res);
    });
}

void GeoHttpServer::setCORSHeaders(httplib::Response &res, const std::string &origin) {
    res.set_header("Access-Control-Allow-Origin", origin);
    res.set_header("Access-Control-Allow-Methods", "POST, DELETE, PUT, GET, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
    res.set_header("Access-Control-Allow-Credentials", "true");
}

void GeoHttpServer::handleGetTotal(const httplib::Request &req, httplib::Response &res) {
    try {
        // 调用业务层获取总数据量
        unsigned long long totalEntries = server->GetGaoDataCount(); // 假设GeoServer有该方法

        // 构造JSON响应
        nlohmann::json response = {
                {"status", "success"},
                {"totalEntries", totalEntries},
                {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
        };

        res.status = 200;
        res.set_content(response.dump(), "application/json");

        // 记录日志
        logger.log(INFO, "成功返回总数据量: " + std::to_string(totalEntries));
    } catch (const std::exception &e) {
        res.status = 500;
        nlohmann::json errorResponse = {
                {"status", "error"},
                {"message", "获取数据量失败: " + std::string(e.what())}
        };
        res.set_content(errorResponse.dump(), "application/json");
        logger.log(ERROR, "获取总数据量失败: " + std::string(e.what()));
    }
}

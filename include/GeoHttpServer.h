/**
 * @Description HTTP服务处理前端业务请求
 * @Version 1.0.0
 * @Date 2025/3/2 16:55
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */
#ifndef GEOSPATIALDATASYSTEM_GEOHTTPSERVER_H
#define GEOSPATIALDATASYSTEM_GEOHTTPSERVER_H

#include "GeoServer.h"
#include "Public.h"
#include <nlohmann/json.hpp>
#include <string>
#include <httplib.h>


class GeoHttpServer {

public:

    explicit GeoHttpServer(GeoServer *server);

    // 处理前端查询请求
    void handleQuery(const httplib::Request &req, httplib::Response &res);

    // 删除单个删除请求
    void handleDelete(const httplib::Request &req, httplib::Response &res);

    // 处理前端随机生成数据请求
    void handleInsertGeoData(const httplib::Request &req, httplib::Response &res);

    // 处理前端随机删除数据请求
    void handleDeleteGeoData(const httplib::Request &req, httplib::Response &res);

    // 处理OPTIONS请求
    void handleOptions(const httplib::Request &req, httplib::Response &res);

    // 处理查询、删除、插入等请求的主逻辑
    void setupRoutes(httplib::Server &server);

private:
    GeoServer *server;

    // 设置CORS头
    void setCORSHeaders(httplib::Response &res, const std::string &origin);
};


#endif //GEOSPATIALDATASYSTEM_GEOHTTPSERVER_H

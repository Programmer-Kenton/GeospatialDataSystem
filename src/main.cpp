/**
 * @Description TODO
 * @Version 1.0.0
 * @Date 2025/2/11 14:33
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */

#include "GeoHttpServer.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <chrono>
#include <iostream>


// 备份代码
int main(int argc, char *argv[]) {

    GeoServer pServer;
    // 项目初始化
    if (!pServer.initSystem()) exit(-1);


    GeoHttpServer server(&pServer);

    httplib::Server httpServer;

    server.setupRoutes(httpServer);

    // 启动监听 8080 端口
    httpServer.listen("0.0.0.0", 8080);

    pServer.destroySystem();

    getchar();
}

/**
 * @Description TODO
 * @Version 1.0.0
 * @Date 2025/2/28 23:37
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */


#include "Public.h"
#include "GeoServer.h"
#include "RTreeManager.h"


GeoServer *pServer = new GeoServer();

int main(){

    pServer->initSystem();

    std::cout << "初始化完成..." << std::endl;

    pServer->insertRTreeData(100,id);

    std::cout << "插入R树后的尺寸为: " << manager->count() << std::endl;
}
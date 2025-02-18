/**
 * @Description TODO
 * @Version 1.0.0
 * @Date 2025/2/11 14:33
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */

#include "GeoServer.h"



GeoServer *pServer = new GeoServer;

int main(int argc,char *argv[]){

    // 项目初始化
    pServer->initSystem();

    id = GeoTools::getInitID(fileName);
    // GeoTools::generate_geospatial_data(10, id);
    
    std::cout << "删除前R树容量为:" << manager->count() << std::endl;


    pServer->deleteRtreeData(id,10);

    std::cout << "删除后R树容量为:" << manager->count() << std::endl;

    std::cout << "删除后R的内容为: " << std::endl;
    manager->printRTree();
    getchar();
}
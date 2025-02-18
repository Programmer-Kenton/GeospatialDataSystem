/**
 * @Description 地理对象类型
 * @Version 1.0.0
 * @Date 2025/2/11 14:30
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */
#ifndef GEOSPATIALDATASYSTEM_GEOOBJECT_H
#define GEOSPATIALDATASYSTEM_GEOOBJECT_H

#include <vector>
#include <string>
#include <boost/geometry.hpp>

namespace bg = boost::geometry;

// 定义地理数据类型
typedef bg::model::point<double, 2, bg::cs::cartesian> Point; // 点
typedef bg::model::box<Point> Box; // 外接矩形
typedef bg::model::linestring<Point> LineString; // 面
typedef bg::model::polygon<Point> Polygon; // 多边形

// 地理对象结构体
struct GeoObject {
    std::string id; // 对象ID
    std::string type; // 几何类型 Point Line Polygon
    std::vector<Point> coordinates; // 坐标列表
    // int status; // 默认为0即程序启动加载进去的数据 1表示后面新增的数据 -1表示删除的数据
};


#endif //GEOSPATIALDATASYSTEM_GEOOBJECT_H

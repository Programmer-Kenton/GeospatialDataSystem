/**
 * @Description TODO
 * @Version 1.0.0
 * @Date 2025/2/11 23:35
 * @Github https://github.com/Programmer-Kenton
 * @Author Kenton
 */

#include "GeoServer.h"
#include "RTreeManager.h"
#include <shared_mutex>



void GeoServer::insertRTreeData(int num, unsigned long long int initID) {
    // 初始化Python解释器
    Py_Initialize();

    // 检查Python解释器是否成功初始化
    if (!Py_IsInitialized()) {
        std::cerr << "Python初始化失败！" << std::endl;
        return;
    }

    try {

        // 设置Python脚本搜索路径
        PyRun_SimpleString("import sys");
        PyRun_SimpleString("sys.path.append('/usr/local/WorkSpace/GeospatialDataSystem/script')");

        // 加载指定的Python模块
        PyObject* pName = PyUnicode_DecodeFSDefault("InsertGeoData");  // 模块名称（不带扩展名）
        PyObject* pModule = PyImport_Import(pName);
        Py_XDECREF(pName);  // 释放pName对象

        // 如果模块加载失败，则打印错误并退出
        if (!pModule) {
            PyErr_Print();  // 打印Python错误信息
            throw std::runtime_error("无法加载Python模块！");
        }

        // 获取Python模块中的函数
        PyObject* pFunc = PyObject_GetAttrString(pModule, "insertRTreeData");
        if (!pFunc || !PyCallable_Check(pFunc)) {
            PyErr_Print();  // 打印Python错误信息
            Py_XDECREF(pModule);  // 释放模块对象
            throw std::runtime_error("未找到可调用的函数！");
        }

        // 构造函数参数元组并调用Python函数
        PyObject* pArgs = PyTuple_Pack(2, PyLong_FromLong(num), PyLong_FromUnsignedLongLong(initID));
        PyObject* pValue = PyObject_CallObject(pFunc, pArgs);
        Py_XDECREF(pArgs);  // 释放参数对象

        // 检查Python函数的返回值是否为空
        if (!pValue) {
            PyErr_Print();  // 打印Python错误信息
            Py_XDECREF(pFunc);
            Py_XDECREF(pModule);
            throw std::runtime_error("调用Python函数失败！");
        }

        // 验证返回值是否为列表类型
        if (!PyList_Check(pValue)) {
            Py_XDECREF(pValue);
            Py_XDECREF(pFunc);
            Py_XDECREF(pModule);
            throw std::runtime_error("Python返回值不是列表！");
        }

        // 遍历Python返回的列表
        Py_ssize_t size = PyList_Size(pValue);

        for (Py_ssize_t i = 0; i < size; ++i){
            PyObject* pItem = PyList_GetItem(pValue, i);  // 获取列表中的元素（借用引用）

            // 检查列表元素是否为二元元组
            if (!PyTuple_Check(pItem) || PyTuple_Size(pItem) != 2) {
                std::cerr << "错误: 无法解析Python返回值的元素！" << std::endl;
                continue;
            }

            // 获取元组中的外接矩形和地理信息
            PyObject* pBoundingBox = PyTuple_GetItem(pItem, 0);  // 外接矩形
            PyObject* pGeoInfo = PyTuple_GetItem(pItem, 1);      // 地理信息字符串

            // 检查外接矩形和地理信息的格式
            if (!PyTuple_Check(pBoundingBox) || PyTuple_Size(pBoundingBox) != 4 || !PyUnicode_Check(pGeoInfo)) {
                std::cerr << "错误: 无效的返回值格式！" << std::endl;
                continue;
            }

            // 解析外接矩形的四个坐标点
            double minx = PyFloat_AsDouble(PyTuple_GetItem(pBoundingBox, 0));
            double miny = PyFloat_AsDouble(PyTuple_GetItem(pBoundingBox, 1));
            double maxx = PyFloat_AsDouble(PyTuple_GetItem(pBoundingBox, 2));
            double maxy = PyFloat_AsDouble(PyTuple_GetItem(pBoundingBox, 3));

            Box bounding_box(Point(minx, miny), Point(maxx, maxy));  // 创建Box对象表示外接矩形

            // 解析地理信息字符串（格式：ID, GeometryType, Status）
            std::string geo_info = PyUnicode_AsUTF8(pGeoInfo);
            std::istringstream ss(geo_info);
            std::string id_str, type_str, status_str,coordinates_str;
            std::getline(ss, id_str, ',');
            std::getline(ss, type_str, ',');
            std::getline(ss, status_str, ',');
            std::getline(ss, coordinates_str);  // 获取Coordinates部分

            // 创建GeoObject对象并填充字段
            GeoObject obj;
            obj.id = id_str;
            obj.type = type_str;              // 几何类型
            // obj.status = std::stoi(status_str); // 转换状态为整数
            obj.coordinates = GeoTools::parseCoordinates(coordinates_str);   // 解析坐标并填充Coordinates字段


            std::ostringstream oss;
            oss << obj.id << "," << obj.type;

            // 使用锁保护共享资源的写操作
            {
                std::shared_mutex &rtree_mutex = manager->getMutex();
                std::lock_guard<std::shared_mutex> lock(rtree_mutex);

                manager->insertRTree(bounding_box, oss.str());  // 插入R树
                insertVec.push_back(obj);  // 添加到地理对象向量
            }
        }

        // 释放返回值和函数对象
        Py_XDECREF(pValue);
        Py_XDECREF(pFunc);
        Py_XDECREF(pModule);

    }catch (const std::exception& e){
        // 捕获并输出异常信息
        std::cerr << "错误: " << e.what() << std::endl;
    }

    // 关闭Python解释器
    Py_Finalize();
}

void GeoServer::deleteRtreeData(unsigned long long int id, int num) {
    GeoTools::generateRandomIds(id,num);
    manager->deleteRTree();
}

void GeoServer::initSystem() {
    // 读取 CSV 文件数据
    std::cout << "程序开始... 单线程读取CSV文件地理信息数据" << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    auto geo_objects = GeoTools::readCSV(fileName);
    if (geo_objects.empty()) {
        std::cerr << "错误: 没有读取到任何地理对象。" << std::endl;
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    std::cout << "读取CSV文件数据结束耗时: " << duration.count() << "秒" << std::endl;

    // 构建R树索引
    std::cout << "程序开始... 计算每个地理信息的最小外接框并构建R树索引(二次分裂算法构建):" << std::endl;
    auto start2 = std::chrono::high_resolution_clock::now();

    manager->buildRTree(geo_objects);

    auto end2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration2 = end2 - start2;
    std::cout << "二次分裂算法 构建R树耗时: " << duration2.count() << "秒" << std::endl;

    // 打印R树内容
    std::cout << "开始打印R树内容..." << std::endl;
    manager->printRTree();

    // 启动CSV定时任务线程
    task.start(); // 启动定时器
}

void GeoServer::queryRTreePolygon() {
    // 获取用户输入
    std::cout << "请输入面坐标 (经度,纬度 经度,纬度 ...): ";
    std::string input;
    std::getline(std::cin, input);

    // 解析坐标
    auto coords = GeoTools::parseCoordinates(input);
    if (coords.size() < 3) {
        std::cerr << "错误: 面查询至少需要三个坐标点。" << std::endl;
    }

    // 确保闭合
    if (!bg::equals(coords.front(), coords.back())) {
        coords.push_back(coords.front());
    }

    // 构造查询面（Polygon）和最小外接矩形（MBR）
    Polygon query_polygon;
    query_polygon.outer().assign(coords.begin(), coords.end());
    Box query_box = bg::return_envelope<Box>(query_polygon);

    // 查询 R 树
    std::cout << "查询开始..." << std::endl;
    auto query_start = std::chrono::high_resolution_clock::now();

    std::vector<Value> result = manager->searchRTree(query_box);

    auto query_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> query_duration = query_end - query_start;
    std::cout << "查询结束 耗时: " << query_duration.count() << "秒" << std::endl;

    // 统计查询结果
    int point_count = 0, line_count = 0, polygon_count = 0;
    for (const auto &item : result) {
        std::istringstream iss(item.second);
        std::string id, type;
        std::getline(iss, id, ',');
        std::getline(iss, type, ',');

        if (type == POINT) point_count++;
        else if (type == LINE) line_count++;
        else if (type == POLYGON) polygon_count++;

        std::cout << "匹配对象: ID=" << id << ", 类型=" << type << std::endl;
    }

    // 打印统计信息
    std::cout << "\n查询结果统计:" << std::endl;
    std::cout << "点的个数: " << point_count << std::endl;
    std::cout << "线的个数: " << line_count << std::endl;
    std::cout << "面的个数: " << polygon_count << std::endl;
}





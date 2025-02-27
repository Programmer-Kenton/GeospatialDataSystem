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

    // 设置线程池
    pool.setMode(PoolMode::MODE_CACHED);
    pool.start(THREAD_INIT_COUNT);

    logger.setLogLevel(INFO);  // 设置日志级别

    // 1. 多线程读取CSV文件
    std::cout << "程序开始... 多线程读取CSV文件地理信息数据" << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<GeoObject> geo_objects;
    std::mutex geo_mutex;  // 保护 geo_objects

    // 计算CSV文件总行数
    size_t totalLines = GeoTools::countLines(fileName); // 获取CSV文件总行数
    size_t batchSize = totalLines / THREAD_INIT_COUNT; // 计算每个线程需要读取的行数

    std::vector<std::future<void>> csvTasks;

    for (int i = 0; i < THREAD_INIT_COUNT; ++i) {
        size_t startLine = i * batchSize + 1; // 多线程读取跳过CSV文件表头
        size_t endLine = (i == THREAD_INIT_COUNT - 1) ? totalLines : startLine + batchSize;

        csvTasks.push_back(pool.submitTask([this, &geo_objects, &geo_mutex, startLine, endLine](){
            auto partial_data = GeoTools::readCSV(fileName, startLine, endLine);
            std::lock_guard<std::mutex> lock(geo_mutex);
            geo_objects.insert(geo_objects.end(), partial_data.begin(), partial_data.end());
        }));
    }

    for (auto &csvTask : csvTasks) {
        csvTask.get();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    logger.log(INFO, "多线程读取CSV文件数据结束，耗时: " + std::to_string(duration.count()) + " 秒");

    // 2. 多线程构建R树
    std::cout << "程序开始... 多线程并行构建R树:" << std::endl;
    auto start2 = std::chrono::high_resolution_clock::now();

    // 调用RTreeManager构建R树
    manager->buildRTree(geo_objects, pool); // 使用 manager 调用 buildRTree

    auto end2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration2 = end2 - start2;
    logger.log(INFO, "多线程并行构建R树耗时: " + std::to_string(duration2.count()) + " 秒");

    // task.start(); // 开启定时任务
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





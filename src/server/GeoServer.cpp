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



bool GeoServer::insertRTreeData(int num, unsigned long long int initID) {

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
        PyObject* pFunc = PyObject_GetAttrString(pModule, "generate_geospatial_data");
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
            // obj.coordinates = GeoTools::parseCoordinates(coordinates_str);   // 解析坐标并填充Coordinates字段


            std::ostringstream oss;
            oss << obj.id << "," << obj.type;

            // 使用锁保护共享资源的写操作
            manager->insertRTree(bounding_box, oss.str());  // 插入R树

            std::lock_guard<std::mutex> lockGuard(insert_mutex);
            insertVec.push_back(obj);  // 添加到地理对象向量

        }

        // 释放返回值和函数对象
        Py_XDECREF(pValue);
        Py_XDECREF(pFunc);
        Py_XDECREF(pModule);

    }catch (const std::exception& e){
        // 捕获并输出异常信息
        std::cerr << "错误: " << e.what() << std::endl;
        return false;
    }

    // 更新全局变量id值
    id += num;
    return true;
}

bool GeoServer::initSystem() {

    logger.log(INFO,"全球大规模地理信息信息数据集系统开始初始化...");

    // 初始化Python解释器
    Py_Initialize();
    // 检查Python解释器是否成功初始化
    if (!Py_IsInitialized()) {
        std::cerr << "Python初始化失败！" << std::endl;
        logger.log(WARNING,"Python解释器初始化失败...");
        return false;
    }


    // 设置线程池为动态扩展模式并启动线程池
    pool.setMode(PoolMode::MODE_CACHED);
    pool.start(THREAD_INIT_COUNT);

    logger.log(INFO,"线程池启动成功，初始线程数量为" + std::to_string(THREAD_INIT_COUNT));

    // 多线程读取CSV文件
    logger.log(INFO,"多线程读取CSV文件地理信息数据...");
    auto start = std::chrono::high_resolution_clock::now();

    // 用于存储CSV文件中读取的所有GeoObject
    std::vector<GeoObject> geoObjects;
    // 保护geoObjects的互斥锁
    std::mutex geoMutex;

    // 计算CSV文件总行数
    size_t totalLines = GeoTools::countLines(fileName); // 获取CSV文件总行数
    if (totalLines <= 1){
        std::cerr << "CSV文件为空或读取失败！" << std::endl;
        logger.log(WARNING,"CSV文件为空或读取失败");
        return false;
    }

    // 假设第一行为表头，所以数据行数为totalLines-1
    size_t dataLines = totalLines - 1;
    // 计算每个线程需要读取的行数（向上取整）
    size_t batchSize = (dataLines + THREAD_INIT_COUNT - 1) / THREAD_INIT_COUNT;

    std::vector<std::future<void>> csvTasks;

    // 遍历所有线程，分批读取CSV数据
    for (int i = 0; i < THREAD_INIT_COUNT; ++i) {
        // 第一个线程需要跳过表头，数据从第2行开始；其它线程则直接计算行号
        size_t startLine = (i == 0) ? 2 : (i * batchSize + 1);

        // 计算结束行号
        size_t endLine = (i == THREAD_INIT_COUNT - 1) ? totalLines : std::min(totalLines, startLine + batchSize - 1);

        // 将任务提交到线程池中执行
        csvTasks.push_back(pool.submitTask([this, &geoObjects, &geoMutex, startLine, endLine]() {
            // 读取CSV文件中指定区间的数据
            auto partialData = GeoTools::readCSV(fileName, startLine, endLine);
            // 加锁后将读取到的数据合并到geoObjects中
            std::lock_guard<std::mutex> lock(geoMutex);
            geoObjects.insert(geoObjects.end(), partialData.begin(), partialData.end());
        }));
    }

    for (auto &csvTask : csvTasks) {
        csvTask.get();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    logger.log(INFO, "多线程读取CSV文件数据结束，耗时: " + std::to_string(duration.count()) + " 秒");

    // 2. 多线程构建R树
    auto start2 = std::chrono::high_resolution_clock::now();

    // 调用RTreeManager构建R树
    manager->buildRTree(geoObjects, pool); // 使用 manager 调用 buildRTree

    auto end2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration2 = end2 - start2;
    logger.log(INFO, "多线程并行构建R树耗时: " + std::to_string(duration2.count()) + " 秒");

    // 获取初始ID值
    id = GeoTools::getInitID(fileName);
    // task.start(); // 开启定时任务

    return true;
}

nlohmann::json GeoServer::queryRTreePolygon(const std::vector<Point> &coords) {
    nlohmann::json response_data = nlohmann::json::object();


    // 坐标检查
    if (coords.size() < 3) {
        response_data["status"] = "error";
        response_data["message"] = "错误: 面查询至少需要三个坐标点。";
        return response_data;
    }

    // 确保闭合
    std::vector<Point> closed_coords = coords;
    if (!bg::equals(closed_coords.front(), closed_coords.back())) {
        closed_coords.push_back(closed_coords.front());
    }



    // 构造查询多边形和最小外接矩形（MBR）
    Polygon query_polygon;
    query_polygon.outer().assign(closed_coords.begin(), closed_coords.end());
    Box query_box = bg::return_envelope<Box>(query_polygon);

    std::cout << "查询开始..." << std::endl;
    auto query_start = std::chrono::high_resolution_clock::now();

    // 查询R树
    std::vector<Value> result = manager->searchRTree(query_box);

    auto query_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> query_duration = query_end - query_start;
    std::cout << "查询结束 耗时: " << query_duration.count() << "秒" << std::endl;

    // 统计结果
    int point_count = 0, line_count = 0, polygon_count = 0;
    nlohmann::json data_array = nlohmann::json::array();

    for (const auto &item : result) {
        std::istringstream iss(item.second);
        std::string id, type;
        std::getline(iss, id, ',');
        std::getline(iss, type, ',');

        if (type == POINT) point_count++;
        else if (type == LINE) line_count++;
        else if (type == POLYGON) polygon_count++;

        nlohmann::json result_item;
        result_item["id"] = id;
        result_item["type"] = type;
        result_item["coordinates"] = manager->box_to_string(item.first);
        data_array.push_back(result_item);
    }

    // 组装 JSON 响应
    response_data["status"] = "success";
    response_data["statistics"] = {
            {"point_count", point_count},
            {"line_count", line_count},
            {"polygon_count", polygon_count}
    };
    response_data["data"] = data_array;
    response_data["query_time"] = query_duration.count();

    return response_data;
}


bool GeoServer::deleteRtreeData(int num, unsigned long long int deleteId) {
    if (deleteId == 0){
        GeoTools::generateRandomIds(num);
    } else if (deleteId > 0){
        {
            std::lock_guard<std::mutex> lock(delete_mutex);
            deleteSet.insert(deleteId);
        }

        {
            std::lock_guard<std::mutex> lock(deleteRTree_mutex);
            deleteRtreeSet.insert(deleteId);
        }
    }

    if (manager->deleteRTree()) return true;
}

void GeoServer::destroySystem() {
    // 关闭Python解释器
    Py_Finalize();
}





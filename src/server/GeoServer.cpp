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

        // 加载 Python 模块
        PyObject* pName = PyUnicode_DecodeFSDefault("InsertGeoData");
        PyObject* pModule = PyImport_Import(pName);
        Py_XDECREF(pName); // 释放 pName

        if (!pModule) {
            PyErr_Print();
            throw std::runtime_error("无法加载 Python 模块！");
        }

        // 获取 Python 生成数据的函数
        PyObject* pFunc = PyObject_GetAttrString(pModule, "generate_geospatial_data");
        if (!pFunc || !PyCallable_Check(pFunc)) {
            PyErr_Print();
            Py_XDECREF(pModule);
            throw std::runtime_error("未找到可调用的 Python 生成数据函数！");
        }

        // 调用 Python 函数
        PyObject* pArgs = PyTuple_Pack(2, PyLong_FromLong(num), PyLong_FromUnsignedLongLong(initID));
        PyObject* pValue = PyObject_CallObject(pFunc, pArgs);
        Py_XDECREF(pArgs);

        if (!pValue || !PyList_Check(pValue)) {
            PyErr_Print();
            Py_XDECREF(pFunc);
            Py_XDECREF(pModule);
            throw std::runtime_error("Python 返回数据格式错误！");
        }

        Py_ssize_t data_size = PyList_Size(pValue);
        if (data_size == 0) {
            Py_XDECREF(pValue);
            Py_XDECREF(pFunc);
            Py_XDECREF(pModule);
            return false;
        }

        // 使用线程池解析&插入数据
        int threadCount = pool.getCurrentThreadCount();
        size_t batchSize = (data_size + threadCount - 1) / threadCount;  // 分片大小

        std::vector<std::future<void>> futures;

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < threadCount; ++i) {
            size_t startIdx = i * batchSize;
            size_t endIdx = std::min(startIdx + batchSize, (size_t)data_size);

            futures.push_back(pool.submitTask([this, startIdx, endIdx, pValue]() {
                std::vector<std::pair<Box, std::string>> batchInsertData;
                std::vector<GeoObject> localGeoObjects;  // 用于批量插入 insertVec

                for (size_t j = startIdx; j < endIdx; ++j) {
                    PyObject* pItem = PyList_GetItem(pValue, static_cast<Py_ssize_t>(j));
                    if (!pItem || !PyTuple_Check(pItem) || PyTuple_Size(pItem) != 2) continue;

                    PyObject* pBoundingBox = PyTuple_GetItem(pItem, 0);
                    PyObject* pGeoInfo = PyTuple_GetItem(pItem, 1);

                    if (!PyTuple_Check(pBoundingBox) || PyTuple_Size(pBoundingBox) != 4 || !PyUnicode_Check(pGeoInfo)) {
                        continue;
                    }

                    double minx = PyFloat_AsDouble(PyTuple_GetItem(pBoundingBox, 0));
                    double miny = PyFloat_AsDouble(PyTuple_GetItem(pBoundingBox, 1));
                    double maxx = PyFloat_AsDouble(PyTuple_GetItem(pBoundingBox, 2));
                    double maxy = PyFloat_AsDouble(PyTuple_GetItem(pBoundingBox, 3));
                    Box bounding_box(Point(minx, miny), Point(maxx, maxy));

                    std::string geo_info = PyUnicode_AsUTF8(pGeoInfo);
                    std::istringstream ss(geo_info);
                    std::string id_str, type_str, status_str, coordinates_str;
                    std::getline(ss, id_str, ',');
                    std::getline(ss, type_str, ',');
                    std::getline(ss, status_str, ',');
                    std::getline(ss, coordinates_str);

                    GeoObject obj;
                    obj.id = id_str;
                    obj.type = type_str;
                    obj.coordinates = GeoTools::parseCoordinates(coordinates_str);

                    std::cout << "新生成的id为: " + obj.id << std::endl;

                    batchInsertData.emplace_back(bounding_box, geo_info);
                    localGeoObjects.push_back(obj);  // 存入局部 insertVec
                }


                // 批量插入R树
                manager->insertBatchRTree(batchInsertData);

                std::lock_guard<std::mutex> lock(insert_mutex);
                insertVec.insert(insertVec.end(), localGeoObjects.begin(), localGeoObjects.end());


            }));
        }

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> duration = end - start;

        logger.log(INFO,"批量插入数据量为: " + std::to_string(num) + " 耗时为: " + std::to_string(duration.count()) + " R树尺寸为: " + std::to_string(manager->count()));

        for (auto &future : futures) {
            future.get();
        }


        // 对全局的 insertVec 进行排序，确保ID按数值递增
        {
            std::lock_guard<std::mutex> lock(insert_mutex);
            std::sort(insertVec.begin(), insertVec.end(), [](const GeoObject &a, const GeoObject &b) {
                return std::stoull(a.id) < std::stoull(b.id);
            });
        }


        Py_XDECREF(pValue);
        Py_XDECREF(pFunc);
        Py_XDECREF(pModule);

    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return false;
    }

    id += num;
    return true;
}

bool GeoServer::initSystem() {

    // loadConfig();

    logger.log(DEBUG,"全球大规模地理信息数据集系统开始初始化...");

    // 初始化Python解释器
    Py_Initialize();
    // 检查Python解释器是否成功初始化
    if (!Py_IsInitialized()) {
        std::cerr << "Python初始化失败！" << std::endl;
        logger.log(WARNING,"Python解释器初始化失败...");
        return false;
    }


    // 设置线程池为动态扩展模式并启动线程池
    pool.setMode(PoolMode::MODE_FIXED);
    pool.start(THREAD_INIT_COUNT);


    // 多线程读取CSV文件
    logger.log(INFO,"多线程读取CSV文件地理信息数据...");
    auto start = std::chrono::high_resolution_clock::now();

    // 用于存储CSV文件中读取的所有GeoObject
    std::vector<GeoObject> geoObjects;
    // 保护geoObjects的互斥锁
    std::mutex geoMutex;

    // 计算CSV文件总行数
    size_t totalLines = GeoTools::countLines(fileName); // 获取CSV文件总行数
    logger.log(INFO,"初始CSV文件总行数为: " + std::to_string(totalLines));

    if (totalLines <= 1){
        std::cerr << "CSV文件为空或读取失败！" << std::endl;
        logger.log(WARNING,"CSV文件为空或读取失败");
        return false;
    }

    // 计算数据行数和分片大小
    size_t dataLines = totalLines - 1;  // 数据行数 = 总行数 - 1 跳过表头
    // 计算每个线程需要读取的行数（向上取整）
    // threadCount 当前线程池中线程数量
    int threadCount = pool.getCurrentThreadCount();
    size_t batchSize = (dataLines + threadCount - 1) / threadCount;

    // 存储线程任务的future对象
    std::vector<std::future<void>> csvTasks;

    // 遍历所有线程，分批读取CSV数据
    for (int i = 0; i < threadCount; ++i) {
        // 按数据行索引分片（0-based）
        size_t startDataIdx = i * batchSize; // 当前线程处理的数据行起始索引
        size_t endDataIdx = std::min(startDataIdx + batchSize, dataLines); // 结束索引（防溢出）

        // 转换为文件行号（1-based）
        size_t startLine = startDataIdx + 2;  // 数据行0 → 文件行2
        size_t endLine = endDataIdx + 1;      // 数据行n → 文件行n+1

        // 确保不超过文件总行数
        endLine = std::min(endLine, totalLines);

        csvTasks.push_back(pool.submitTask([this, &geoObjects, &geoMutex, startLine, endLine]() {
            try {
                // 线程任务：读取指定行范围的CSV数据
                auto partialData = GeoTools::readCSV(fileName, startLine, endLine);
                // 加锁合并数据
                std::lock_guard<std::mutex> lock(geoMutex);
                geoObjects.insert(geoObjects.end(), partialData.begin(), partialData.end());
            } catch (const std::exception &e) {
                logger.log(ERROR, "CSV读取任务异常: " + std::string(e.what()));
            }
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

    // 调用RTreeManager构建R树 采用局部构建与全局合并策略
    manager->buildRTree(geoObjects, pool); // 使用 manager 调用 buildRTree

    auto end2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration2 = end2 - start2;
    logger.log(INFO, "多线程并行构建R树耗时: " + std::to_string(duration2.count()) + " 秒");

    // 获取初始ID值
    id = dataLines;

    task.start(); // 开启定时任务

    return true;
}

nlohmann::json GeoServer::queryRTreePolygon(const std::vector<Point> &coords) {
    nlohmann::json response_data = nlohmann::json::object();


    // 坐标检查 至少需要三个点构成多边形
    if (coords.size() < 3) {
        response_data["status"] = "error";
        response_data["message"] = "错误: 面查询至少需要三个坐标点。";
        return response_data;
    }

    // 确保闭合 如果首尾不一致，添加首点到末尾
    std::vector<Point> closed_coords = coords;
    if (!bg::equals(closed_coords.front(), closed_coords.back())) {
        closed_coords.push_back(closed_coords.front());
    }



    // 构造查询多边形和最小外接矩形（MBR）
    Polygon query_polygon;
    query_polygon.outer().assign(closed_coords.begin(), closed_coords.end());
    Box query_box = bg::return_envelope<Box>(query_polygon);


    auto query_start = std::chrono::high_resolution_clock::now();

    // 查询R树
    std::vector<Value> result = manager->searchRTree(query_box);

    auto query_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> query_duration = query_end - query_start;


    size_t totalResults = result.size();

    logger.log(INFO,"处理前端查询请求... 共查询到" + std::to_string(totalResults) + "条数据 耗时: " + std::to_string(query_duration.count()) + "秒");


    // 并行处理查询结果，利用线程池将结果分段处理
    // 定义局部任务返回结构
    struct PartialResult {
        nlohmann::json jsonArray; // 本段查询结果的 JSON 数组
        int pointCount;
        int lineCount;
        int polygonCount;
    };


    int poolThreadCount = pool.getCurrentThreadCount();  // 获取线程池中线程数量

    // 分段数取二者较小值，确保每段至少有一条数据
    size_t segmentCount = std::min((size_t)poolThreadCount, totalResults);
    if (segmentCount == 0) segmentCount = 1;
    size_t segmentSize = (totalResults + segmentCount - 1) / segmentCount;

    std::vector<std::future<PartialResult>> futures;
    for (size_t i = 0; i < segmentCount; ++i){
        size_t startIdx = i * segmentSize;
        size_t endIdx = std::min(startIdx + segmentSize, totalResults);
        // 提交分段任务到线程池
        futures.push_back(pool.submitTask([this, &result, startIdx, endIdx]() -> PartialResult {
            PartialResult pr;
            pr.jsonArray = nlohmann::json::array();
            pr.pointCount = 0;
            pr.lineCount = 0;
            pr.polygonCount = 0;
            // 遍历本段数据
            for (size_t j = startIdx; j < endIdx; ++j) {
                const auto &item = result[j];
                // 解析 item.second 中的 id 和 type（格式为 "id,type"）
                std::istringstream iss(item.second);
                std::string id, type;
                std::getline(iss, id, ',');
                std::getline(iss, type, ',');
                if (type == POINT) pr.pointCount++;
                else if (type == LINE) pr.lineCount++;
                else if (type == POLYGON) pr.polygonCount++;

                // 构造 JSON 对象记录结果数据
                nlohmann::json obj;
                obj["id"] = id;
                obj["type"] = type;
                // 利用 manager->box_to_string 将 Box 转换为字符串形式
                obj["coordinates"] = manager->box_to_string(item.first);
                pr.jsonArray.push_back(obj);
            }
            return pr;
        }));
    }


    // 合并所有分段任务的结果
    int totalPointCount = 0, totalLineCount = 0, totalPolygonCount = 0;
    nlohmann::json mergedData = nlohmann::json::array();
    for (auto &fut : futures) {
        PartialResult pr = fut.get();
        totalPointCount += pr.pointCount;
        totalLineCount += pr.lineCount;
        totalPolygonCount += pr.polygonCount;
        // 将本段 JSON 数组合并到总结果中
        for (const auto &elem : pr.jsonArray) {
            mergedData.push_back(elem);
        }
    }

    // 组装最终JSON响应
    response_data["status"] = "success";
    response_data["statistics"] = {
            {"point_count", totalPointCount},
            {"line_count", totalLineCount},
            {"polygon_count", totalPolygonCount}
    };
    response_data["data"] = mergedData;
    response_data["query_time"] = query_duration.count();

    return response_data;
}


bool GeoServer::deleteRtreeData(int num, unsigned long long deleteId) {
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

    if (manager->deleteRTree(pool)) return true;
}

void GeoServer::destroySystem() {
    // 关闭Python解释器
    Py_Finalize();
    logger.log(INFO,"系统关闭...销毁资源完成");
}

bool GeoServer::loadConfig() {
    const std::string configPath = "/usr/local/WorkSpace/GeospatialDataSystem/etc/config.json";

    try{
        // 打开配置文件
        std::ifstream configStream(configPath);
        if (!configStream.is_open()){
            std::cerr << "错误：无法打开配置文件 " << configPath << std::endl;
            return false;
        }

        // 2. 解析JSON内容
        nlohmann::json config;
        configStream >> config;  // 从文件流直接解析JSON

        // 3. 提取字段并赋值给全局变量
        fileName = config.at("DataFile").get<std::string>();
        // scriptFile = config.at("ScriptFile").get<std::string>();
        logger.setLogFile(config.at("GeoLog").get<std::string>());

        return true;
    }catch(const nlohmann::json::exception& e) {
        // 处理JSON解析异常（如字段缺失、类型错误）
        std::cerr << "JSON解析错误: " << e.what() << std::endl;
        return false;
    }catch (const std::exception& e){
        // 处理其他异常（如文件读取失败）
        std::cerr << "系统错误: " << e.what() << std::endl;
        return false;
    }
}

unsigned long long GeoServer::GetGaoDataCount() {
    return manager->count();
}





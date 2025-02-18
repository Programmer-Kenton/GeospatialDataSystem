import random
from shapely.geometry import Point, LineString, Polygon


# 生成随机地理坐标点（经度, 纬度）
def generate_random_point():
    lat = random.uniform(-90, 90)  # 纬度范围 [-90, 90]
    lon = random.uniform(-180, 180)  # 经度范围 [-180, 180]
    return Point(lon, lat)


# 生成随机线（由多个随机点组成）
def generate_random_line(num_points=5):
    points = [generate_random_point() for _ in range(num_points)]
    return LineString([point.coords[0] for point in points])


# 生成随机面（由多个随机点组成，且保证构成封闭形状）
def generate_random_polygon(num_points=6):
    points = [generate_random_point() for _ in range(num_points)]
    coords = [(point.coords[0][0], point.coords[0][1]) for point in points]
    # 保证面是闭合的
    if coords[0] != coords[-1]:
        coords.append(coords[0])
    return Polygon(coords)


# 随机选择几何类型
def generate_random_geometry():
    geometry_type = random.choice(['Point', 'Line', 'Polygon'])
    if geometry_type == 'Point':
        return generate_random_point(), geometry_type
    elif geometry_type == 'Line':
        return generate_random_line(), geometry_type
    elif geometry_type == 'Polygon':
        return generate_random_polygon(), geometry_type


# 生成随机地理信息数据并返回R树值
def generate_geospatial_data(num_records, init_id):
    """
    生成地理信息数据并返回给调用方，用于插入R树。
    :param num_records: 需要生成的记录数量
    :param init_id: 初始ID号
    :return: 包含地理数据的列表，每项为 (外接矩形, 地理信息字符串)
    """
    data = []
    record_id = init_id  # 从初始ID开始
    for _ in range(num_records):
        record_id += 1  # 确保 ID 从初始值递增
        # print("record_id = " + record_id)
        geometry, geom_type = generate_random_geometry()

        # 根据几何类型生成坐标字符串
        if geom_type == 'Point':
            coordinates = f"{geometry.x:.6f},{geometry.y:.6f}"
        elif geom_type == 'Line':
            coordinates = ' '.join([f"{coord[0]:.6f},{coord[1]:.6f}" for coord in geometry.coords[:]])
        elif geom_type == 'Polygon':
            coordinates = ' '.join([f"{coord[0]:.6f},{coord[1]:.6f}" for coord in geometry.exterior.coords[:-1]])

        # 创建地理信息记录
        record = {
            'ID': record_id,
            'GeometryType': geom_type,
            'Coordinates': coordinates,
            'Status': 1  # 新生成数据的状态为1
        }

        # 计算几何对象的外接矩形
        minx, miny, maxx, maxy = geometry.bounds
        bounding_box = (minx, miny, maxx, maxy)  # 外接矩形

        # 返回数据，用于C++插入R树
        geo_info = f"{record['ID']},{record['GeometryType']},{record['Status']},{record['Coordinates']}"
        data.append((bounding_box, geo_info))

    return data
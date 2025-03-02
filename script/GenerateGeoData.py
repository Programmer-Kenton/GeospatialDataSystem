import random
import pandas as pd
from shapely.geometry import Point, LineString, Polygon
import os
import csv


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


# 生成数据集并保存为CSV文件
def generate_geospatial_dataset(num_records=100000):
    data = []
    for record_id in range(1, num_records + 1):
        geometry, geom_type = generate_random_geometry()
        if geom_type == 'Point':
            record = {
                'ID': record_id,
                'GeometryType': geom_type,
                'Coordinates': f"{geometry.x:.6f},{geometry.y:.6f}"
            }
        elif geom_type == 'Line':
            record = {
                'ID': record_id,
                'GeometryType': geom_type,
                'Coordinates': ' '.join([f"{coord[0]:.6f},{coord[1]:.6f}" for coord in geometry.coords[:]])
            }
        elif geom_type == 'Polygon':
            record = {
                'ID': record_id,
                'GeometryType': geom_type,
                'Coordinates': ' '.join([f"{coord[0]:.6f},{coord[1]:.6f}" for coord in geometry.exterior.coords[:-1]])
                # 去掉最后重复闭合点
            }
        data.append(record)

    df = pd.DataFrame(data)

    # 设置文件输出路径
    output_dir = 'D:\\Graduate\\GeospatialDataSystem\\data'
    os.makedirs(output_dir, exist_ok=True)
    file_path = os.path.join(output_dir, 'geospatial_data.csv')

    # 使用 quoting=csv.QUOTE_MINIMAL，并不使用 escapechar
    df.to_csv(file_path, index=False, quoting=csv.QUOTE_MINIMAL, escapechar=None)

    print(f"Generated {num_records} records and saved to '{file_path}'.")

# 生成并保存数据集
generate_geospatial_dataset()

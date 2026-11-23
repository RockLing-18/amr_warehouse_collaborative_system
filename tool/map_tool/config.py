import os


# 默认edge_server地址
DEFAULT_EDGE_SERVER = "http://127.0.0.1:8080"

# 地图必须文件
REQUIRED_WAREHOUSE_FILES = [
    "zones.yaml",
    "stations.yaml",
    "walls.yaml"
]

# 地图可选文件：无 narrow 区域时允许不存在
OPTIONAL_WAREHOUSE_FILES = [
    "traffic_gates.yaml"
]

# 临时输出目录
PACKAGE_DIR = "output"

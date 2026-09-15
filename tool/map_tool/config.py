import os


# 默认edge_server地址
DEFAULT_EDGE_SERVER = "http://127.0.0.1:8080"

# 地图必须文件
REQUIRED_FILES = [
    "map.yaml",
    "map.pgm",
    "zone.yaml",
    "station.yaml"
]

# 临时输出目录
PACKAGE_DIR = "output"
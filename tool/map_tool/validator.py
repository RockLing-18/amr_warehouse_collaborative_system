import os
import glob
import yaml

from config import REQUIRED_WAREHOUSE_FILES


def find_map_yaml(map_dir):
    candidates = []
    candidates.extend(sorted(glob.glob(os.path.join(map_dir, "*.yaml"))))
    candidates.extend(sorted(glob.glob(os.path.join(map_dir, "*.yml"))))
    if not candidates:
        return None
    return candidates[0]


def load_map_image_name(map_yaml_path):
    try:
        with open(map_yaml_path, "r", encoding="utf-8") as f:
            data = yaml.safe_load(f)
        return data.get("image", "") if isinstance(data, dict) else ""
    except Exception:
        return ""


def discover_warehouse_files(warehouse_dir):
    map_dir = os.path.join(warehouse_dir, "map")
    map_yaml_path = find_map_yaml(map_dir)
    if not map_yaml_path:
        print(f"[ERROR] map yaml not found in {map_dir}")
        return None

    image_name = load_map_image_name(map_yaml_path)
    if not image_name:
        print(f"[ERROR] map yaml missing image field:{map_yaml_path}")
        return None

    image_path = os.path.join(map_dir, image_name)
    if not os.path.exists(image_path):
        print(f"[ERROR] map image not found:{image_path}")
        return None

    files = {
        "map_yaml": os.path.basename(map_yaml_path),
        "map_image": image_name,
    }

    for file in REQUIRED_WAREHOUSE_FILES:
        file_path = os.path.join(warehouse_dir, file)
        if not os.path.exists(file_path):
            print(f"[ERROR] missing warehouse file:{file_path}")
            return None
        key = file.replace(".yaml", "")
        files[key] = file

    return files


def validate_warehouse_directory(warehouse_dir):
    if not os.path.exists(warehouse_dir):
        print(f"[ERROR] warehouse directory not exist:{warehouse_dir}")
        return False

    files = discover_warehouse_files(warehouse_dir)
    if files is None:
        return False

    print(f"[OK] map yaml: {files['map_yaml']}")
    print(f"[OK] map image: {files['map_image']}")
    for name in ["zones", "stations", "walls"]:
        print(f"[OK] {files[name]}")
    return True

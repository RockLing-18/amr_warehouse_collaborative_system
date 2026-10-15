import os
import yaml
from datetime import datetime


def generate_metadata(warehouse_dir, warehouse_id, version, files):
    metadata = {
        "warehouse_id": warehouse_id,
        "version": version,
        "create_time": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "files": {
            "map": files["map_yaml"],
            "image": files["map_image"],
            "zone": files["zones"],
            "station": files["stations"],
            "wall": files["walls"],
        },
    }

    path = os.path.join(warehouse_dir, "metadata.yaml")
    with open(path, "w", encoding="utf-8") as f:
        yaml.dump(
            metadata,
            f,
            allow_unicode=True,
            sort_keys=False,
        )

    print(f"[OK] generate {path}")
    return path

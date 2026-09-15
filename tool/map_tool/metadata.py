import yaml
from datetime import datetime
import os


def generate_metadata(
        map_dir,
        warehouse_id,
        version):


    metadata = {

        "warehouse_id":
            warehouse_id,


        "version":
            version,


        "create_time":
            datetime.now().strftime(
                "%Y-%m-%d %H:%M:%S"
            ),


        "files":
        {
            "map":
                "map.yaml",

            "image":
                "map.pgm",

            "zone":
                "zone.yaml",

            "station":
                "station.yaml"
        }
    }


    path = os.path.join(
        map_dir,
        "metadata.yaml"
    )


    with open(
        path,
        "w",
        encoding="utf-8"
    ) as f:

        yaml.dump(
            metadata,
            f,
            allow_unicode=True,
            sort_keys=False
        )


    print(
        f"[OK] generate {path}"
    )


    return path
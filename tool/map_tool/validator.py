import os
from config import REQUIRED_MAP_FILES, REQUIRED_WAREHOUSE_FILES

def validate_warehouse_directory(path):

    if not os.path.exists(path):
        print(
            f"[ERROR] warehouse directory not exist:{path}"
        )
        return False

    map_dir = os.path.join(path, "map")

    for file in REQUIRED_MAP_FILES:

        file_path = os.path.join(
            map_dir,
            file
        )

        if not os.path.exists(file_path):

            print(
                f"[ERROR] missing map file:{file_path}"
            )

            return False

        print(
            f"[OK] {file}"
        )

    for file in REQUIRED_WAREHOUSE_FILES:

        file_path = os.path.join(path, file)

        if not os.path.exists(file_path):

            print(
                f"[ERROR] missing warehouse file:{file_path}"
            )

            return False

        print(
            f"[OK] {file}"
        )

    return True

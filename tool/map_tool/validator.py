import os
from config import REQUIRED_FILES

def validate_map_directory(path):

    if not os.path.exists(path):
        print(
            f"[ERROR] map directory not exist:{path}"
        )
        return False


    for file in REQUIRED_FILES:

        file_path = os.path.join(
            path,
            file
        )

        if not os.path.exists(file_path):

            print(
                f"[ERROR] missing file:{file}"
            )

            return False

        print(
            f"[OK] {file}"
        )


    return True
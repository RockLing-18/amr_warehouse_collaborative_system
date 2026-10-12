import zipfile
import os


def package_map(
        warehouse_dir,
        warehouse_id,
        version,
        output_dir):


    os.makedirs(
        output_dir,
        exist_ok=True
    )


    zip_path = os.path.join(
        output_dir,
        f"{warehouse_id}_{version}.zip"
    )


    map_dir = os.path.join(warehouse_dir, "map")

    with zipfile.ZipFile(
            zip_path,
            "w",
            zipfile.ZIP_DEFLATED) as z:

        for file in ["map.yaml", "map.pgm"]:
            file_path = os.path.join(map_dir, file)
            if os.path.exists(file_path):
                z.write(file_path, file)

        for file in ["zones.yaml", "stations.yaml", "metadata.yaml"]:
            file_path = os.path.join(warehouse_dir, file)
            if os.path.exists(file_path):
                z.write(file_path, file)


    print(
        f"[OK] package:{zip_path}"
    )


    return zip_path

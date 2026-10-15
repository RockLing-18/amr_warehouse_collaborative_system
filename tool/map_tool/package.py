import os
import zipfile


def package_map(warehouse_dir, warehouse_id, version, files, output_dir):
    os.makedirs(output_dir, exist_ok=True)

    zip_path = os.path.join(
        output_dir,
        f"{warehouse_id}_{version}.zip",
    )

    map_dir = os.path.join(warehouse_dir, "map")

    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
        for src_name in [files["map_yaml"], files["map_image"]]:
            src_path = os.path.join(map_dir, src_name)
            if os.path.exists(src_path):
                z.write(src_path, src_name)

        for name in ["zones", "stations", "walls"]:
            file_name = files[name]
            src_path = os.path.join(warehouse_dir, file_name)
            if os.path.exists(src_path):
                z.write(src_path, file_name)

        metadata_name = "metadata.yaml"
        metadata_path = os.path.join(warehouse_dir, metadata_name)
        if os.path.exists(metadata_path):
            z.write(metadata_path, metadata_name)

    print(f"[OK] package:{zip_path}")
    return zip_path

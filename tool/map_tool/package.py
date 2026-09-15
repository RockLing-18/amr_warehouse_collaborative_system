import zipfile
import os


def package_map(
        map_dir,
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


    with zipfile.ZipFile(
            zip_path,
            "w",
            zipfile.ZIP_DEFLATED) as z:


        for root, dirs, files in os.walk(map_dir):

            for file in files:

                file_path = os.path.join(
                    root,
                    file
                )


                # 计算相对路径
                rel_path = os.path.relpath(
                    file_path,
                    map_dir
                )


                # zip里面增加warehouse目录
                arc_name = os.path.join(
                    warehouse_id,
                    rel_path
                )


                z.write(
                    file_path,
                    arc_name
                )


    print(
        f"[OK] package:{zip_path}"
    )


    return zip_path
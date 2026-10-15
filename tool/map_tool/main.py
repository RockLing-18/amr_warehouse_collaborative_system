import os


from validator import validate_warehouse_directory, discover_warehouse_files
from metadata import generate_metadata
from package import package_map
from uploader import upload_map


from config import (
    DEFAULT_EDGE_SERVER,
    PACKAGE_DIR
)



def main():
    print(
        "===== AMR Map Tool ====="
    )

    warehouse_root=input(
        "Warehouse root directory:"
    ).strip()

    warehouse_id=input(
        "warehouse_id:"
    ).strip()


    version=input(
        "version:"
    ).strip()

    if not warehouse_id or not version:
        print(
            "invalid parameter"
        )
        return

    warehouse_dir = os.path.join(warehouse_root, warehouse_id)
    if not validate_warehouse_directory(warehouse_dir):
        return

    files = discover_warehouse_files(warehouse_dir)
    if files is None:
        return

    generate_metadata(
        warehouse_dir,
        warehouse_id,
        version,
        files
    )


    zip_file=package_map(
        warehouse_dir,
        warehouse_id,
        version,
        files,
        PACKAGE_DIR
    )


    upload=input(
        "Upload to edge server?(y/n):"
    )


    if upload.lower()!="y":

        print(
            "finish"
        )

        return



    url=input(
        f"upload url(default:{DEFAULT_EDGE_SERVER}):"
    ).strip()


    if not url:

        url=DEFAULT_EDGE_SERVER



    api=url+"/api/maps/upload"



    activate=input(
        "activate now?(y/n):"
    )


    result=upload_map(
        api,
        zip_file,
        warehouse_id,
        version,
        activate.lower()=="y"
    )


    if result:

        print(
            "upload success"
        )

    else:

        print(
            "upload failed"
        )



if __name__=="__main__":

    main()

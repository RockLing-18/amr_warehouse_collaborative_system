import requests


def upload_map(
        url,
        zip_file,
        warehouse_id,
        version,
        activate):


    params={

        "warehouse_id":
            warehouse_id,


        "version":
            version,


        "activate":
            str(activate).lower()
    }


    print(
        "upload:",
        zip_file
    )


    with open(
        zip_file,
        "rb"
    ) as f:


        files={

            "file":
            (
                zip_file,
                f,
                "application/zip"
            )

        }


        response=requests.post(
            url,
            params=params,
            files=files
        )


    print(
        response.text
    )


    return response.status_code == 200
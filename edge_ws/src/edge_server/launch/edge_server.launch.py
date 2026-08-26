from launch import LaunchDescription
from launch_ros.actions import Node

import os

from ament_index_python.packages import get_package_share_directory


def generate_launch_description():

    pkg = get_package_share_directory(
            "edge_server"
        )


    config = os.path.join(
            pkg,
            "config",
            "edge.yaml"
        )


    return LaunchDescription([
        Node(
            package="edge_server",
            executable="edge_server",
            name="edge_server",
            output="screen",
            parameters=[
                config
            ]

        )

    ])
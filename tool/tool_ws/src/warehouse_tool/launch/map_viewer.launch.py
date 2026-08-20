import os

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch.actions import OpaqueFunction

from launch_ros.actions import Node

from ament_index_python.packages import get_package_share_directory


def launch_setup(context):

    pkg_dir = get_package_share_directory(
        "warehouse_tool"
    )

    # 地图文件
    map_yaml = LaunchConfiguration(
        "map_yaml"
    ).perform(context)

    use_sim_time = LaunchConfiguration(
        "use_sim_time"
    ).perform(context)

    # RViz 配置
    rviz_config = os.path.join(
        pkg_dir,
        "rviz",
        "warehouse_editor.rviz"
    )

    # Map Server
    map_server = Node(
        package="nav2_map_server",
        executable="map_server",
        name="map_server",
        parameters=[
            {
                "yaml_filename": map_yaml,
                "use_sim_time": use_sim_time,
            }
        ],
        output="screen"
    )

    # Lifecycle Manager
    lifecycle_manager = Node(
        package="nav2_lifecycle_manager",
        executable="lifecycle_manager",
        name="lifecycle_manager",
        parameters=[
            {
                "use_sim_time": use_sim_time,
                "autostart": True,
                "node_names": [
                    "map_server"
                ],
            }
        ],
        output="screen"
    )

    # RViz
    rviz = Node(
        package="rviz2",
        executable="rviz2",
        name="rviz2",
        arguments=[
            "-d",
            rviz_config
        ],
        parameters=[
            {
                "use_sim_time": use_sim_time
            }
        ],
        output="screen"
    )

    return [
        map_server,
        lifecycle_manager,
        rviz
    ]


def generate_launch_description():

    return LaunchDescription([

        DeclareLaunchArgument(
            "map_yaml",
            description="Warehouse map path"
        ),

        DeclareLaunchArgument(
            "use_sim_time",
            default_value="true"
        ),

        OpaqueFunction(
            function=launch_setup
        )
    ])
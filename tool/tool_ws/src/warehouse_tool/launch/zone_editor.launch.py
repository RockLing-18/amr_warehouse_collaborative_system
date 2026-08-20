from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():

    map_dir = LaunchConfiguration("map_dir")

    zone_editor = Node(
        package="warehouse_tool",
        executable="zone_editor",
        name="zone_editor",
        parameters=[
            {
                "map_dir": map_dir
            }
        ],
        output="screen"
    )

    return LaunchDescription([
        DeclareLaunchArgument(
            "map_dir",
            description="Warehouse map directory"
        ),
        zone_editor
    ])
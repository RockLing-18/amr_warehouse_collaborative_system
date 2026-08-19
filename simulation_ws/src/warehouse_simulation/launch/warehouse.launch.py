import os
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    pkg_dir = get_package_share_directory(
        "warehouse_simulation"
    )

    world_file = os.path.join(
        pkg_dir,
        "worlds",
        "warehouse.world"
    )

    gazebo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(
                get_package_share_directory(
                    "gazebo_ros"
                ),
                "launch",
                "gazebo.launch.py"
            )
        ),
        launch_arguments={
            "world": world_file
        }.items()
    )


    simulation_manager = Node(
        package="simulation_manager",
        executable="simulation_manager",
        name="simulation_manager",
        output="screen"
    )

    return LaunchDescription([
        gazebo,
        simulation_manager
    ])
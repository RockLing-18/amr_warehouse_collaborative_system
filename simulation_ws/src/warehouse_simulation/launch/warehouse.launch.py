import os
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch.actions import OpaqueFunction
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import yaml

def load_yaml(path):

    with open(path, "r") as f:
        return yaml.safe_load(f)

def launch_setup(context):

    pkg_dir = get_package_share_directory(
        "warehouse_simulation"
    )

    config_name = LaunchConfiguration(
        "config"
    ).perform(context)

    config_file = os.path.join(
        pkg_dir,
        "config",
        config_name
    )

    config = load_yaml(config_file)
    simulation = config["simulation"]

    world_file = simulation["world"]["file"]
    if not os.path.isabs(world_file):
        world_file = os.path.join(
            pkg_dir,
            world_file
        )

    gazebo_config = simulation["gazebo_config"]

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
            "world": world_file,
            "verbose": str(gazebo_config["verbose"]).lower()
        }.items()
    )

    edge_server_config = simulation["edge_server"]

    simulation_manager = Node(
        package="simulation_manager",
        executable="simulation_manager",
        name="simulation_manager",
        parameters=[
            {
                "warehouse_id":simulation["warehouse_id"],
                "websocket_url":edge_server_config["websocket"]["url"],
                "sync_interval":simulation["robot_sync"]["interval"]
            }
        ],
        output="screen"
    )

    return [
        gazebo,
        simulation_manager
    ]

def generate_launch_description():

    return LaunchDescription([
        DeclareLaunchArgument(
            "config",
            default_value="simulation.yaml",
            description="simulation config file"
        ),
        OpaqueFunction(
            function=launch_setup
        )
    ])
import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    robot_id = LaunchConfiguration("robot_id")
    use_sim_time = LaunchConfiguration("use_sim_time")

    amr_agent_share = get_package_share_directory("amr_agent")
    amr_behavior_manager_share = get_package_share_directory("amr_behavior_manager")

    agent_cfg = os.path.join(amr_agent_share, "config", "amr_config.yaml")
    behavior_cfg = os.path.join(
        amr_behavior_manager_share,
        "config",
        "amr_behavior_manager.yaml",
    )

    return LaunchDescription(
        [
            DeclareLaunchArgument("robot_id", default_value="robot_01"),
            DeclareLaunchArgument("use_sim_time", default_value="true"),
            Node(
                package="amr_agent",
                executable="amr_agent",
                name="amr_agent",
                namespace=robot_id,
                parameters=[{"cfg_path": agent_cfg}],
                output="screen",
            ),
            Node(
                package="amr_behavior_manager",
                executable="amr_behavior_manager_node",
                name="amr_behavior_manager",
                namespace=robot_id,
                parameters=[behavior_cfg, {"use_sim_time": use_sim_time}],
                output="screen",
            ),
            Node(
                package="amr_navigation",
                executable="navigation_server",
                name="navigation_server",
                namespace=robot_id,
                parameters=[
                    {
                        "use_sim_time": use_sim_time,
                        "pose_publish_rate_hz": 1.0,
                    }
                ],
                output="screen",
            ),
        ]
    )

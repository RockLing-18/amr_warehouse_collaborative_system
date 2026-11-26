import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, OpaqueFunction
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def _make_nodes(robot_id, use_sim_time, launch_nav2, use_bt_execution):
    use_sim_time_bool = use_sim_time.lower() == "true"
    launch_nav2_bool = launch_nav2.lower() == "true"
    use_bt_execution_bool = use_bt_execution.lower() == "true"

    amr_agent_share = get_package_share_directory("amr_agent")
    amr_behavior_manager_share = get_package_share_directory("amr_behavior_manager")

    agent_cfg = os.path.join(amr_agent_share, "config", "amr_config.yaml")
    behavior_cfg = os.path.join(
        amr_behavior_manager_share,
        "config",
        "amr_behavior_manager.yaml",
    )

    amr_agent_node = Node(
        package="amr_agent",
        executable="amr_agent",
        name="amr_agent",
        namespace=robot_id,
        parameters=[{"cfg_path": agent_cfg}],
        output="screen",
    )
    amr_behavior_manager_node = Node(
        package="amr_behavior_manager",
        executable="amr_behavior_manager_node",
        name="amr_behavior_manager",
        namespace=robot_id,
        parameters=[
            behavior_cfg,
            {
                "use_sim_time": use_sim_time_bool,
                "use_bt_execution": use_bt_execution_bool,
            },
        ],
        output="screen",
    )
    navigation_server_node = Node(
        package="amr_navigation",
        executable="navigation_server",
        name="navigation_server",
        namespace=robot_id,
        parameters=[
            {
                "use_sim_time": use_sim_time_bool,
                "pose_publish_rate_hz": 1.0,
                "launch_nav2": launch_nav2_bool,
            }
        ],
        output="screen",
    )

    return [amr_agent_node, amr_behavior_manager_node, navigation_server_node]


def launch_setup(context):
    robot_id = LaunchConfiguration("robot_id").perform(context)
    use_sim_time = LaunchConfiguration("use_sim_time").perform(context)
    launch_nav2 = LaunchConfiguration("launch_nav2").perform(context)
    use_bt_execution = LaunchConfiguration("use_bt_execution").perform(context)
    separate_terminals = LaunchConfiguration("separate_terminals").perform(context)

    if separate_terminals.lower() == "true":
        return [
            ExecuteProcess(
                cmd=[
                    "ros2",
                    "run",
                    "amr_bringup",
                    "start_amr",
                    "--robot-id",
                    robot_id,
                    "--use-sim-time",
                    use_sim_time,
                    "--launch-nav2",
                    launch_nav2,
                    "--use-bt-execution",
                    use_bt_execution,
                ],
                output="screen",
            )
        ]

    return _make_nodes(robot_id, use_sim_time, launch_nav2, use_bt_execution)


def generate_launch_description():
    return LaunchDescription(
        [
            DeclareLaunchArgument("robot_id", default_value="robot_01"),
            DeclareLaunchArgument("use_sim_time", default_value="true"),
            DeclareLaunchArgument("launch_nav2", default_value="false"),
            DeclareLaunchArgument("use_bt_execution", default_value="false"),
            DeclareLaunchArgument("separate_terminals", default_value="false"),
            OpaqueFunction(function=launch_setup),
        ]
    )

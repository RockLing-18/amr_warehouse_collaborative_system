from launch import LaunchDescription
from launch_ros.actions import Node
# 封装终端指令相关类--------------
from launch.actions import ExecuteProcess
# from launch.substitutions import FindExecutable
# 参数声明与获取-----------------
from launch.actions import DeclareLaunchArgument
# 文件包含相关-------------------
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
# 分组相关----------------------
# from launch_ros.actions import PushRosNamespace
# from launch.actions import GroupAction
# 事件相关----------------------
from launch.event_handlers import OnProcessStart, OnProcessExit
from launch.actions import RegisterEventHandler,LogInfo
# 获取功能包下share目录路径-------
from ament_index_python.packages import get_package_share_directory

from launch_ros.parameter_descriptions import ParameterValue
from launch.substitutions import Command,LaunchConfiguration
import os
def generate_launch_description():
    """
    AD
    """
    pkg_dir = get_package_share_directory("amr_description")
    default_model_path = os.path.join(pkg_dir,"urdf/amr_description","amr.urdf.xacro")
    # default_rviz_path = os.path.join(pkg_dir,"rviz","display.rviz")
    default_gazebo_world_path = os.path.join(pkg_dir,"world","custom_room.world")
    model = DeclareLaunchArgument(name="model", default_value=default_model_path)

    p_value = ParameterValue(Command(["xacro ", LaunchConfiguration("model")]))
    robot_state_publisher_node = Node(
        package="robot_state_publisher",
        executable="robot_state_publisher",
        parameters=[{"robot_description": p_value}]
    )

    # joint_state_publisher_node = Node(
    # package="joint_state_publisher",
    # executable="joint_state_publisher",
    # parameters=[
    #     {"ignore_missing_joint_goals": True},
    #     {"use_sim_time": True}
    # ],
    # output="screen"
    # )


    action_launch_gazebo= IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(
            get_package_share_directory("gazebo_ros"),
            "launch",
            "gazebo.launch.py"
            )
        ),
        launch_arguments=[("world", default_gazebo_world_path),("verbose","true")]
    )

    action_spawn_entity= Node(
        package="gazebo_ros",
        executable="spawn_entity.py",
        arguments=["-topic", "/robot_description", "-entity", "amr_robot"]
    )

    action_load_joint_state_controller = ExecuteProcess(
        cmd="ros2 control load_controller amr_robot_joint_state_broadcaster --set-state active".split(" "),
        output="screen"
    )

    action_load_effort_controller = ExecuteProcess(
        cmd="ros2 control load_controller amr_effort_controller --set-state active".split(" "),
        output="screen"
    )

    action_load_diff_driver_controller = ExecuteProcess(
        cmd="ros2 control load_controller amr_diff_drive_controller --set-state active".split(" "),
        output="screen"
    )


    return LaunchDescription([
                        model, 
                        robot_state_publisher_node,
                        action_launch_gazebo,
                        action_spawn_entity,
                        RegisterEventHandler(
                            event_handler=OnProcessExit(
                                target_action=action_spawn_entity,
                                on_exit=[action_load_joint_state_controller]
                            )
                        ),
                        RegisterEventHandler(
                            event_handler=OnProcessExit(
                                target_action=action_load_joint_state_controller,
                                on_exit=[action_load_diff_driver_controller]
                            )
                        )
    ])
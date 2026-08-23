from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, OpaqueFunction, RegisterEventHandler, TimerAction
from launch.event_handlers import OnProcessExit
from launch.substitutions import Command, LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue
from ament_index_python.packages import get_package_share_directory

import os
import tempfile
import yaml
import copy


def load_yaml(path):
    """读取 YAML 配置"""
    with open(path, 'r', encoding='utf-8') as f:
        return yaml.safe_load(f)


def generate_controller_yaml(spec, robot_id, prefix):
    """控制器模板 + 机器人规格覆盖：生成该机器人的控制器参数文件，返回路径"""
    pkg_dir = get_package_share_directory('amr_description')
    template_path = os.path.join(pkg_dir, 'config', 'amr_ros2_controller.yaml')
    template = load_yaml(template_path)

    robot = spec['robot']

    # 原始 controller 配置
    controller_manager_cfg = copy.deepcopy(
        template['controller_manager']
    )

    diff_controller_cfg = copy.deepcopy(
        template['amr_diff_drive_controller']
    )
    # controller_manager_cfg = template['controller_manager']
    # diff_controller_cfg = template['amr_diff_drive_controller']

    left_joint = prefix + robot['wheels']['left_joint']
    right_joint = prefix + robot['wheels']['right_joint']

    controller_manager_key = f'/{robot_id}/controller_manager'
    diff_controller_key = f'/{robot_id}/amr_diff_drive_controller'

    # 几何/关节参数由规格文件注入
    # 修改机器人相关参数
    diff = diff_controller_cfg['ros__parameters']
    diff['left_wheel_names'] = [
        prefix + robot['wheels']['left_joint']
    ]

    diff['right_wheel_names'] = [
        prefix + robot['wheels']['right_joint']
    ]

    diff['wheel_radius'] = robot['wheel']['radius']
    diff['wheel_separation'] = robot['wheel_separation']

    diff['odom_frame_id'] = (
        prefix + robot['odom_frame']
    )

    diff['base_frame_id'] = (
        prefix + robot['base_frame']
    )

     # 生成最终 namespace
    cfg = {
        f'/{robot_id}/controller_manager': controller_manager_cfg,
        f'/{robot_id}/amr_diff_drive_controller': diff_controller_cfg,
    }

    # 固定路径 /tmp/{robot_id}_controller.yaml
    filename = f"{robot_id}_controller.yaml"
    path = os.path.join("/tmp", filename)

    # 写入yaml，覆盖旧文件
    with open(path, 'w', encoding='utf-8') as f:
        yaml.safe_dump(cfg, f, allow_unicode=True, sort_keys=False)
    return path

    # fd, path = tempfile.mkstemp(suffix='.yaml', prefix=robot_id + '_controller_')
    # with os.fdopen(fd, 'w', encoding='utf-8') as f:
    #     yaml.safe_dump(cfg, f, allow_unicode=True, sort_keys=False)
    # return path


def launch_robot(context, *args, **kwargs):
    """读取规格 -> 生成控制器参数 -> 展开 xacro -> spawn -> 激活控制器"""
    robot_id = LaunchConfiguration('robot_id').perform(context)
    instance_id = LaunchConfiguration('instance_id').perform(context)
    model = LaunchConfiguration('model').perform(context)
    spec_path = LaunchConfiguration('spec').perform(context)
    x = LaunchConfiguration('x').perform(context)
    y = LaunchConfiguration('y').perform(context)
    yaw = LaunchConfiguration('yaw').perform(context)

    spec = load_yaml(spec_path)
    prefix = robot_id + '_'
    controller_yaml = generate_controller_yaml(spec, robot_id, prefix)
    
    print(
    f"[spawn_amr] robot_id={robot_id}, "
    f"instance_id={instance_id}, "
    f"controller_yaml={controller_yaml}"
    )

    robot_spec = spec['robot']
    chassis = robot_spec['chassis']
    wheel = robot_spec['wheel']

    print(
    f"[spawn_amr] robot_id={robot_id}, "
    f"prefix={prefix}, "
    f"controller_yaml={controller_yaml}"
    )

    # xacro 展开：几何参数来自规格文件，prefix / namespace 来自 robot_id
    robot_description = ParameterValue(
        Command([
            'xacro ', model,
            ' prefix:=', prefix,
            ' namespace:=', robot_id,
            ' car_length:=', str(chassis['length']),
            ' car_width:=', str(chassis['width']),
            ' car_height:=', str(chassis['height']),
            ' wheel_radius:=', str(wheel['radius']),
            ' wheel_length:=', str(wheel['width']),
            ' wheel_mass:=', str(wheel['mass']),
            ' controller_params:=', controller_yaml,
        ]),
        value_type=str
    )

    robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        namespace=robot_id,
        parameters=[
            {'robot_description': robot_description},
            {'use_sim_time': True},
        ],
        output='screen'
    )

    spawn_entity = Node(
        package='gazebo_ros',
        executable='spawn_entity.py',
        arguments=[
            '-topic', '/' + robot_id + '/robot_description',
            '-entity', 'amr_' + robot_id + '_' + instance_id,
            # '-robot_namespace', robot_id,
            '-x', x,
            '-y', y,
            '-Y', yaw,
        ],
        output='screen'
    )

    # ros2_control 控制器激活：在 robot_id 命名空间下执行
    # def controller_cmd(controller_name):
    #     return [
    #         'ros2', 'control', 'load_controller', controller_name,
    #         '--set-state', 'active',
    #         '--ros-args', '-r', '__ns:=/' + robot_id,
    #     ]

    # load_joint_state = ExecuteProcess(
    #     cmd=controller_cmd('amr_robot_joint_state_broadcaster'),
    #     output='screen'
    # )

    # load_diff_drive = ExecuteProcess(
    #     cmd=controller_cmd('amr_diff_drive_controller'),
    #     output='screen'
    # )

    load_joint_state = Node(
        package='controller_manager',
        executable='spawner',
        arguments=[
            'amr_robot_joint_state_broadcaster',
            '--controller-manager',
            '/' + robot_id + '/controller_manager',
            '--param-file', controller_yaml,
        ],
        output='screen'
    )

    load_diff_drive = Node(
        package='controller_manager',
        executable='spawner',
        arguments=[
            'amr_diff_drive_controller',
            '--controller-manager',
            '/' + robot_id + '/controller_manager',
            '--param-file', controller_yaml,
        ],
        output='screen'
    )


    return [
        robot_state_publisher,
        spawn_entity,
        RegisterEventHandler(
            OnProcessExit(target_action=spawn_entity, on_exit=[load_joint_state])
        ),
        RegisterEventHandler(
            OnProcessExit(target_action=load_joint_state, on_exit=[load_diff_drive])
        ),
    ]


def generate_launch_description():
    """生成单台 AMR 实例（可重复调用，每台机器人一次）

    用法（需先启动 Gazebo world）：
      ros2 launch amr_description spawn_amr.launch.py robot_id:=robot01 instance_id:=00001 x:=0.0 y:=0.0 yaw:=0.0

    参数来源优先级：config/amr_v1.yaml（机器人规格）> 模板 amr_ros2_controller.yaml（控制参数）
    """
    pkg_dir = get_package_share_directory('amr_description')
    default_model_path = os.path.join(pkg_dir, 'urdf', 'amr.urdf.xacro')
    default_spec_path = os.path.join(pkg_dir, 'config', 'amr_v1.yaml')

    return LaunchDescription([
        DeclareLaunchArgument('robot_id', default_value='robot01'),
        DeclareLaunchArgument('instance_id', default_value='00001'),
        DeclareLaunchArgument('model', default_value=default_model_path),
        DeclareLaunchArgument('spec', default_value=default_spec_path),
        DeclareLaunchArgument('x', default_value='0.0'),
        DeclareLaunchArgument('y', default_value='0.0'),
        DeclareLaunchArgument('yaw', default_value='0.0'),
        OpaqueFunction(function=launch_robot),
    ])

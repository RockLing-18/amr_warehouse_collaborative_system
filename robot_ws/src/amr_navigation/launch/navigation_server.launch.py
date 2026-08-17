from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    
    navigation_server_node = Node(
    package="amr_navigation",
    executable="navigation_server",
    name="navigation_server",
    parameters=[
        {"use_sim_time": True}
    ],
    output="screen"
    )



    return LaunchDescription([navigation_server_node])
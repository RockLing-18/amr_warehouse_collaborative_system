"""在独立终端中分别启动 AMR 的三个节点，便于调试时查看各自日志。"""

import argparse
import os
import shlex
import subprocess
from pathlib import Path

from ament_index_python.packages import get_package_prefix
from ament_index_python.packages import get_package_share_directory


def find_workspace_setup():
    prefix = Path(get_package_prefix("amr_bringup"))
    candidates = [
        prefix / "setup.bash",
        prefix.parent / "setup.bash",
    ]
    for setup in candidates:
        if setup.exists():
            return str(setup)
    raise RuntimeError("Cannot locate robot_ws setup.bash from amr_bringup prefix")


def launch_terminal(title, inner_cmd, ws_setup, ros_distro):
    ros_setup = f"/opt/ros/{ros_distro}/setup.bash"
    bash_cmd = (
        f"source {shlex.quote(ros_setup)} && "
        f"source {shlex.quote(ws_setup)} && "
        f"{inner_cmd}; exec bash"
    )
    try:
        subprocess.Popen(
            [
                "gnome-terminal",
                "--title", title,
                "--",
                "bash", "-i", "-c", bash_cmd,
            ]
        )
    except FileNotFoundError:
        print(
            f"[ERROR] gnome-terminal not found, cannot open {title}. "
            f"Run manually: {inner_cmd}",
            file=os.sys.stderr,
        )


def main():
    parser = argparse.ArgumentParser(
        description="在独立终端中启动 AMR 三个节点"
    )
    parser.add_argument("--robot-id", required=True)
    parser.add_argument("--use-sim-time", default="true")
    parser.add_argument("--launch-nav2", default="false")
    args = parser.parse_args()

    robot_id = args.robot_id
    agent_cfg = os.path.join(
        get_package_share_directory("amr_agent"),
        "config",
        "amr_config.yaml",
    )
    behavior_cfg = os.path.join(
        get_package_share_directory("amr_behavior_manager"),
        "config",
        "amr_behavior_manager.yaml",
    )

    commands = [
        (
            f"amr_agent-{robot_id}",
            "ros2 run amr_agent amr_agent --ros-args "
            f"-r __ns:=/{robot_id} -p cfg_path:={shlex.quote(agent_cfg)}",
        ),
        (
            f"amr_behavior_manager-{robot_id}",
            "ros2 run amr_behavior_manager amr_behavior_manager_node --ros-args "
            f"-r __ns:=/{robot_id} --params-file {shlex.quote(behavior_cfg)} "
            f"-p robot_id:={robot_id} -p use_sim_time:={args.use_sim_time}",
        ),
        (
            f"navigation_server-{robot_id}",
            "ros2 run amr_navigation navigation_server --ros-args "
            f"-r __ns:=/{robot_id} -p launch_nav2:={args.launch_nav2} "
            f"-p use_sim_time:={args.use_sim_time}",
        ),
    ]

    ws_setup = find_workspace_setup()
    ros_distro = os.environ.get("ROS_DISTRO", "humble")

    for title, command in commands:
        print(f"[INFO] Opening {title}")
        print(f"  {command}")
        launch_terminal(title, command, ws_setup, ros_distro)

    print("[OK] AMR nodes launched in separate terminals.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

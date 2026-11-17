#!/usr/bin/env python3
"""在独立终端中启动指定 AMR 的 Nav2 节点。

与 tool_ws 的 warehouse_tool.py 类似，本脚本负责：
1. 根据 robot_id 生成带 TF 帧前缀的 nav2 参数文件；
2. 在 gnome-terminal 中执行 ros2 launch amr_navigation navigation2.launch.py，
   使 Nav2 日志与 navigation_server 日志分离，便于查看。
"""

import argparse
import os
import subprocess
from pathlib import Path

from ament_index_python.packages import get_package_prefix
from ament_index_python.packages import get_package_share_directory


DEFAULT_MAP_YAML = "/opt/amr/data/amr_runtime/map/map.yaml"


def quote_arg(value):
    return f'"{value}"'


def generate_params_file(robot_id, template_path):
    """把 nav2 参数模板中的 <robot_prefix> 替换为 robot_id_。"""
    template = Path(template_path)
    content = template.read_text(encoding="utf-8")
    content = content.replace("<robot_prefix>", f"{robot_id}_")
    content = content.replace("<robot_namespace>", robot_id)

    params_file = Path(f"/tmp/amr_nav2_params_{robot_id}.yaml")
    params_file.write_text(content, encoding="utf-8")
    return str(params_file)


def find_workspace_setup():
    """根据 amr_navigation 的安装前缀定位 robot_ws 的 setup.bash。"""
    prefix = Path(get_package_prefix("amr_navigation"))
    candidates = [
        prefix / "setup.bash",
        prefix.parent / "setup.bash",
    ]
    for setup in candidates:
        if setup.exists():
            return str(setup)
    return None


def main():
    parser = argparse.ArgumentParser(
        description="在独立终端中启动 AMR Nav2"
    )
    parser.add_argument(
        "robot_id",
        help="机器人业务身份，例如 robot01",
    )
    parser.add_argument(
        "--map",
        default=DEFAULT_MAP_YAML,
        help="地图 YAML 路径",
    )
    parser.add_argument(
        "--use-sim-time",
        default="true",
        choices=["true", "false"],
        help="是否使用仿真时钟",
    )
    parser.add_argument(
        "--autostart",
        default="false",
        choices=["true", "false"],
        help="是否让 lifecycle_manager 自动激活 Nav2",
    )
    parser.add_argument(
        "--terminal-title",
        default="Nav2",
        help="新终端标题",
    )
    args = parser.parse_args()

    robot_id = args.robot_id
    template_path = os.path.join(
        get_package_share_directory("amr_navigation"),
        "config",
        "nav2_params.yaml",
    )
    params_file = generate_params_file(robot_id, template_path)

    ws_setup = find_workspace_setup()
    if not ws_setup:
        print(
            "[ERROR] Cannot locate robot_ws setup.bash from amr_navigation prefix.",
            file=os.sys.stderr,
        )
        return 1

    ros_distro = os.environ.get("ROS_DISTRO", "humble")
    ros_setup = f"/opt/ros/{ros_distro}/setup.bash"

    launch_cmd = [
        "ros2", "launch",
        "amr_navigation",
        "navigation2.launch.py",
        f"map:={args.map}",
        f"params_file:={params_file}",
        f"use_sim_time:={args.use_sim_time}",
        f"namespace:={robot_id}",
        f"autostart:={args.autostart}",
    ]
    inner_cmd = " ".join(quote_arg(item) for item in launch_cmd)
    bash_cmd = (
        f"source {quote_arg(ros_setup)} && "
        f"source {quote_arg(ws_setup)} && "
        f"{inner_cmd}; exec bash"
    )

    print(f"[INFO] Generated params file: {params_file}")
    print(f"[INFO] Map: {args.map}")
    print("[INFO] Opening Nav2 in a new terminal:")
    print("  " + " ".join(launch_cmd))

    terminal_cmd = [
        "gnome-terminal",
        "--title", args.terminal_title,
        "--",
        "bash", "-i", "-c", bash_cmd,
    ]
    try:
        subprocess.Popen(terminal_cmd)
    except FileNotFoundError:
        print(
            "[ERROR] gnome-terminal not found. "
            "Run the command below in a separate terminal manually:",
            file=os.sys.stderr,
        )
        print("  " + " ".join(launch_cmd), file=os.sys.stderr)
        return 1

    print("[OK] Nav2 terminal launched.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

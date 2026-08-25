#!/usr/bin/env python3

import os
import sys
import subprocess
from pathlib import Path
import signal
from ament_index_python.packages import get_package_share_directory
from ament_index_python.packages import get_package_prefix

import yaml


class WarehouseTool:
    def __init__(self, config_path: str):

        self.config_path = Path(config_path).resolve()

        if not self.config_path.exists():
            raise FileNotFoundError(
                f"Config file not found: {self.config_path}"
            )

        self.config = self._load_yaml(self.config_path)sync

        self._load_config()

        self.map_process: subprocess.Popen | None = None
        # self.editor_processes: list[subprocess.Popen] = []
    
    def cleanup(self):
        # 关闭 map viewer
        if self.map_process and self.map_process.poll() is None:
            self.map_process.terminate()
            try:
                self.map_process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.map_process.kill()
        self.map_process = None

    # ============================================================
    # Configuration
    # ============================================================

    @staticmethod
    def _load_yaml(path: Path):

        with path.open("r", encoding="utf-8") as f:
            data = yaml.safe_load(f)

        if not data:
            raise RuntimeError(
                f"Empty configuration file: {path}"
            )

        return data

    def _load_config(self):

        warehouse = self.config.get("warehouse", {})
        edge_server = self.config.get("edge_server", {})

        self.warehouse_id = warehouse.get("id")

        if not self.warehouse_id:
            raise RuntimeError(
                "warehouse.id is not configured"
            )

        data_root = warehouse.get("data_root")

        if not data_root:
            raise RuntimeError(
                "warehouse.data_root is not configured"
            )

        self.data_root = Path(data_root).expanduser().resolve()

        self.warehouse_dir = (
            self.data_root / self.warehouse_id
        )

        
        self.map_dir = (
            self.warehouse_dir / "map"
        )

        map_name = warehouse.get("map_name")

        self.map_yaml = (
            self.map_dir / map_name
        )

        self.edge_server_url = edge_server.get(
            "url",
            ""
        )

    # ============================================================
    # Main menu
    # ============================================================

    def run(self):

        self.print_header()

        self.run_map_viewer()

        while True:

            self.print_menu()

            try:
                choice = input("\nSelect: ").strip()
            except KeyboardInterrupt:
                print("\n")
                break

            if choice == "1":
                self.run_zone_editor()

            elif choice == "2":
                self.run_station_editor()

            elif choice == "3":
                self.run_wall_editor()

            elif choice == "4":
                self.show_status()

            elif choice == "5":
                self.upload_data()

            elif choice == "6":
                self.upload_all()

            elif choice == "0":
                print("\nBye.")
                break

            else:
                print("\nInvalid selection.")

    # ============================================================
    # UI
    # ============================================================

    def print_header(self):

        print()
        print("=" * 60)
        print("              AMR Warehouse Tool")
        print("=" * 60)

        print(f"Warehouse : {self.warehouse_id}")
        print(f"Data Root : {self.warehouse_dir}")

        print("=" * 60)

    def print_menu(self):

        print()
        print("-" * 60)
        print("1. Zone Editor")
        print("2. Station Editor")
        print("3. Wall Editor")
        print("4. Show Warehouse Status")
        print("5. Upload Data")
        print("6. Upload All")
        print("0. Exit")
        print("-" * 60)

    def run_process_background(self, command: list[str]) -> subprocess.Popen:
        """后台启动进程，丢弃标准输出与错误，不阻塞主菜单"""
        print("\nCommand:")
        print(" ".join(f'"{arg}"' if " " in arg else arg for arg in command))
        p = subprocess.Popen(
            command,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )
        return p

    def run_map_viewer(self):
        if self.map_process and self.map_process.poll() is None:
            print("\n[INFO] Map Viewer is already running!")
            return
        print("\n[INFO] Starting persistent Map Viewer (map_server + rviz)...")
        cmd = [
            "ros2", "launch",
            "warehouse_tool",
            "map_viewer.launch.py",
            f"map_yaml:={self.map_yaml}"
        ]
        self.map_process = self.run_process_background(cmd)
        print("[OK] Map Viewer running in background, ready for editors.") 

    # ============================================================
    # Editor
    # ============================================================

    def run_zone_editor(self):
        print()
        print("=" * 60)
        print("Starting Zone Editor")
        print("=" * 60)
        if not self.check_map():
            return

        command = [
            "ros2", "run", "warehouse_tool", "zone_editor",
            "--ros-args", "-p", f"map_dir:={self.warehouse_dir}"
        ]
        self.launch_in_new_terminal(command, title="Zone Editor")
        print("[INFO] Zone Editor opened in new terminal!")

    def run_station_editor(self):
        print()
        print("=" * 60)
        print("Starting Station Editor")
        print("=" * 60)
        if not self.check_map():
            return

        command = [
            "ros2", "run", "warehouse_tool", "station_editor",
            "--ros-args", "-p", f"map_dir:={self.warehouse_dir}"
        ]
        self.launch_in_new_terminal(command, title="Station Editor")
        print("[INFO] Station Editor opened in new terminal!")

    def run_wall_editor(self):
        print()
        print("=" * 60)
        print("Starting Wall Editor")
        print("=" * 60)
        if not self.check_map():
            return

        command = [
            "ros2", "run", "warehouse_tool", "wall_editor",
            "--ros-args", "-p", f"map_dir:={self.warehouse_dir}"
        ]
        self.launch_in_new_terminal(command, title="Wall Editor")
        print("[INFO] Wall Editor opened in new terminal!")

    # ============================================================
    # Process
    # ============================================================

    def run_process(self, command):

        print()
        print("Command:")

        print(" ".join(
            f'"{arg}"' if " " in arg else arg
            for arg in command
        ))

        print()

        try:

            # process = subprocess.Popen(
            #     command
            # )
            process = subprocess.Popen(
                command,
                stdin=None
            )

            return_code = process.wait()

            print()

            if return_code == 0:
                print(
                    "Editor exited successfully."
                )
            else:
                print(
                    f"Editor exited with code: {return_code}"
                )

        except FileNotFoundError as e:

            print(
                f"\nFailed to start command: {e}"
            )

            print(
                "Please check ROS2 environment and "
                "workspace installation."
            )

        except KeyboardInterrupt:

            print(
                "\nStopping editor..."
            )

            process.terminate()

            try:
                process.wait(timeout=5)

            except subprocess.TimeoutExpired:

                process.kill()

    def launch_in_new_terminal(self, cmd_list, title="ROS Editor"):
        inner_cmd = " ".join(f'"{item}"' for item in cmd_list)
        prefix = Path(get_package_prefix("warehouse_tool"))  # xxx/install
        ws_install = prefix.parent / "setup.bash"

        bash_cmd = (
            f'source /opt/ros/humble/setup.bash && '
            f'source {ws_install} && '
            f'{inner_cmd}; exec bash'
        )
        full_cmd = [
            "gnome-terminal",
            "--title", title,
            "--",
            "bash", "-i", "-c", bash_cmd
        ]
        subprocess.Popen(full_cmd)

    # ============================================================
    # Validation
    # ============================================================

    def check_map(self):

        if not self.map_dir.exists():

            print()
            print(
                f"Map directory does not exist:"
            )

            print(
                f"  {self.map_dir}"
            )

            return False

        map_yaml = list(
            self.map_dir.glob("*.yaml")
        )

        map_pgm = list(
            self.map_dir.glob("*.pgm")
        )

        if not map_yaml:

            print()
            print(
                "Map YAML file not found."
            )

            print(
                f"  {self.map_dir}"
            )

            return False

        if not map_pgm:

            print()
            print(
                "Map image (.pgm) not found."
            )

            print(
                f"  {self.map_dir}"
            )

            return False

        return True

    # ============================================================
    # Status
    # ============================================================

    def show_status(self):

        print()
        print("=" * 60)
        print("Warehouse Status")
        print("=" * 60)

        print(
            f"Warehouse ID : {self.warehouse_id}"
        )

        print(
            f"Root         : {self.warehouse_dir}"
        )

        self._print_data_status(
            "Map",
            self.map_dir
        )

        print("=" * 60)

    @staticmethod
    def _print_data_status(
        name: str,
        directory: Path
    ):

        exists = directory.exists()

        if exists:

            files = list(
                directory.iterdir()
            )

            print(
                f"{name:<10}: OK "
                f"({len(files)} files)"
            )

        else:

            print(
                f"{name:<10}: NOT FOUND"
            )

    # ============================================================
    # Upload
    # ============================================================

    def upload_data(self):

        print()
        print("=" * 60)
        print("Upload Data")
        print("=" * 60)

        print()
        print("1. Upload Map")
        print("2. Upload Zone")
        print("3. Upload Station")
        print("4. Upload Wall")
        print("0. Back")

        choice = input("\nSelect: ").strip()

        if choice == "1":
            self.upload_map()

        elif choice == "2":
            self.upload_zone()

        elif choice == "3":
            self.upload_station()

        elif choice == "4":
            self.upload_wall()

    def upload_map(self):

        print()
        print("Map upload is not implemented yet.")

        # TODO:
        #
        # POST /api/warehouse/map
        #
        # 上传：
        #   warehouse_id
        #   map.yaml
        #   map.pgm
        #
        # Edge Server负责：
        #   version
        #   校验
        #   持久化
        #   云端同步

    def upload_zone(self):

        print()
        print("Zone upload is not implemented yet.")

        # TODO:
        # Edge Server upload

    def upload_station(self):

        print()
        print("Station upload is not implemented yet.")

        # TODO:
        # Edge Server upload

    def upload_wall(self):

        print()
        print("Wall upload is not implemented yet.")

        # TODO:
        # Edge Server upload

    def upload_all(self):

        print()
        print("=" * 60)
        print("Upload All Warehouse Data")
        print("=" * 60)

        self.show_status()

        confirm = input(
            "\nUpload all data to Edge Server? [y/N]: "
        ).strip().lower()

        if confirm != "y":

            print("Upload cancelled.")

            return

        print()
        print("Upload all is not implemented yet.")

        # TODO:
        #
        # 1. Validate all data
        # 2. Package data
        # 3. Upload to Edge Server
        # 4. Edge Server creates versions
        # 5. Return synchronized version information


tool_global: WarehouseTool | None = None
def sigint_handler(signum, frame):
    global tool_global
    if tool_global:
        tool_global.cleanup()
    print("\n[EXIT] Ctrl+C captured, exit.")
    sys.exit(0)


def main():
    global tool_global
    pkg_share = Path(get_package_share_directory("warehouse_tool"))
    default_config = pkg_share / "config" / "warehouse_tool.yaml"
    config_path = default_config

    config_path = default_config

    if len(sys.argv) > 1:

        config_path = Path(
            sys.argv[1]
        )

    try:

        tool = WarehouseTool(
            str(config_path)
        )
        tool_global = tool
        signal.signal(signal.SIGINT, sigint_handler)

        tool.run()
        tool.cleanup()

    except Exception as e:

        print(
            f"\nERROR: {e}"
        )

        return 1

    return 0


if __name__ == "__main__":

    sys.exit(main())
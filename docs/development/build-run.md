# 构建与启动说明

## 1. 环境要求

- Ubuntu 22.04
- ROS2 Humble
- Gazebo 11 / `gazebo_ros`
- Nav2
- EMQX
- C++17、CMake
- Paho MQTT C++
- yaml-cpp
- libwebsockets
- libzip
- SQLite

## 2. Edge Server 构建

```bash
cd /home/zyr/code/amr_warehouse_collaborative_system/edge_server
cmake -S . -B build
cmake --build build -j2
```

## 3. ROS2 Workspace 构建

### robot_ws

```bash
source /opt/ros/humble/setup.bash
cd /home/zyr/code/amr_warehouse_collaborative_system/robot_ws

rm -rf build install log
colcon build --merge-install --symlink-install
source install/setup.bash
```

### simulation_ws

```bash
source /opt/ros/humble/setup.bash
cd /home/zyr/code/amr_warehouse_collaborative_system/simulation_ws

rm -rf build install log
colcon build --merge-install --symlink-install
source install/setup.bash
```

### tool_ws

```bash
source /opt/ros/humble/setup.bash
cd /home/zyr/code/amr_warehouse_collaborative_system/tool/tool_ws

rm -rf build install log
colcon build --merge-install --symlink-install
source install/setup.bash
```

## 4. 启动 EMQX

根据实际安装方式启动两个 Broker：

- Cloud-side EMQX：后续版本使用。
- Edge-side EMQX：V1 使用。

示例：

```bash
emqx start
```

## 5. 启动 Edge Server

```bash
cd /home/zyr/code/amr_warehouse_collaborative_system
./edge_server/bin/edge_server
```

默认读取配置：

```text
edge_server/config/edge_server.yaml
```

## 6. 启动 AMR

```bash
source /opt/ros/humble/setup.bash
source /home/zyr/code/amr_warehouse_collaborative_system/robot_ws/install/setup.bash

ros2 launch amr_bringup amr_bringup.launch.py \
  robot_id:=robot_01 \
  use_sim_time:=true
```

`amr_bringup` 默认只启动 `amr_agent`、`amr_behavior_manager`、`navigation_server`，
`navigation_server` 收到 `amr_agent` 发布的 `MapInfo` 后，会用其中的 `map_yaml`
自动调用 `scripts/start_nav2.py`，该脚本按 `robot_id` 生成
`/tmp/amr_nav2_params_{robot_id}.yaml`，并在新开终端中运行
`ros2 launch amr_navigation navigation2.launch.py`。因此启动顺序与旧版本一致，
Nav2 日志又与 `navigation_server` 日志分离。

如新终端未自动弹出，可手动触发一次（地图路径由 `navigation_server` 给出）：

```bash
source /opt/ros/humble/setup.bash
source /home/zyr/code/amr_warehouse_collaborative_system/robot_ws/install/setup.bash

ros2 run amr_navigation start_nav2.py robot_01 \
  --map /opt/amr/data/amr_runtime/map/map.yaml
```

调试时如果希望 `amr_agent`、`amr_behavior_manager`、`navigation_server` 也各自
独立终端显示日志，加 `separate_terminals:=true`：

```bash
ros2 launch amr_bringup amr_bringup.launch.py \
  robot_id:=robot01 \
  use_sim_time:=true \
  separate_terminals:=true
```

默认 `separate_terminals:=false` 时，三个节点仍共用执行 `ros2 launch` 的终端。

如需恢复由 `navigation_server` 在本进程内 fork 拉起 Nav2 的旧行为：

```bash
ros2 launch amr_bringup amr_bringup.launch.py \
  robot_id:=robot_01 \
  launch_nav2:=true
```

多 AMR 时，分别启动对应机器人：

```bash
ros2 launch amr_bringup amr_bringup.launch.py robot_id:=robot_02
```

## 7. 启动 Simulation

```bash
source /opt/ros/humble/setup.bash
source /home/zyr/code/amr_warehouse_collaborative_system/simulation_ws/install/setup.bash

ros2 launch warehouse_simulation warehouse.launch.py
```

## 8. 关键参数

### amr_agent

```text
robot.id
edge_server.host
edge_server.port
map.path
pending_report_path
```

### amr_behavior_manager

```text
robot_id
status_publish_rate_hz
task_status_period_s
grasp_simulate_seconds
place_simulate_seconds
carry_capacity
```

### amr_navigation

```text
pose_publish_rate_hz
use_sim_time
```

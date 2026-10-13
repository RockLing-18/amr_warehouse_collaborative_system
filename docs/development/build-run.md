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
cd /home/zyr/code/amr_edge_cloud_platform/edge_server
cmake -S . -B build
cmake --build build -j2
```

## 3. ROS2 Workspace 构建

### robot_ws

```bash
source /opt/ros/humble/setup.bash
cd /home/zyr/code/amr_edge_cloud_platform/robot_ws

rm -rf build install log
colcon build --merge-install --symlink-install
source install/setup.bash
```

### simulation_ws

```bash
source /opt/ros/humble/setup.bash
cd /home/zyr/code/amr_edge_cloud_platform/simulation_ws

rm -rf build install log
colcon build --merge-install --symlink-install
source install/setup.bash
```

### tool_ws

```bash
source /opt/ros/humble/setup.bash
cd /home/zyr/code/amr_edge_cloud_platform/tool/tool_ws

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
cd /home/zyr/code/amr_edge_cloud_platform
./edge_server/bin/edge_server
```

默认读取配置：

```text
edge_server/config/edge_server.yaml
```

## 6. 启动 AMR

```bash
source /opt/ros/humble/setup.bash
source /home/zyr/code/amr_edge_cloud_platform/robot_ws/install/setup.bash

ros2 launch amr_bringup amr_bringup.launch.py \
  robot_id:=robot_01 \
  use_sim_time:=true
```

多 AMR 时：

```bash
ros2 launch amr_bringup amr_bringup.launch.py robot_id:=robot_02
```

## 7. 启动 Simulation

```bash
source /opt/ros/humble/setup.bash
source /home/zyr/code/amr_edge_cloud_platform/simulation_ws/install/setup.bash

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

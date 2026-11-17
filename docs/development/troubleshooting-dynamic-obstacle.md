# 动态障碍物未进入代价图的排查记录

**状态：** 已解决
**范围：** `amr_navigation` / Nav2 / Gazebo 多机仿真
**关键词：** ROS2 namespace、Nav2 costmap、LaserScan、`ros2 topic info`、动态避障

## 1. 现象

AMR（`robot01`）执行完 `UNLOAD` 任务后返回充电点，途中与另一台 AMR（`robot02`）相遇：

- `robot01` 不减速、不绕行，直接撞向 `robot02`。
- RViz 中 `local_costmap` / `global_costmap` 只显示地图自带的静态障碍（墙体、建图时的箱子），看不到 `robot02`。
- Nav2 日志反复出现：

```text
Failed to make progress
Running spin / backup / wait
```

- 最终 `amr_behavior_manager` 报 `return home failed after retries`。
- 但 `/robot01/scan` 本身有正常的激光数据。

## 2. 排查过程

### 2.1 先排除 TF 问题

之前已经解决过 `map` / `odom` 帧缺失问题。此时用：

```bash
ros2 run tf2_ros tf2_echo map robot01_base_footprint \
  --ros-args -r /tf:=/robot01/tf -r /tf_static:=/robot01/tf_static
```

能持续输出正确位姿，说明 TF 链路正常。

### 2.2 检查激光数据

```bash
ros2 topic echo /robot01/scan --once
```

输出包含大量有效 `ranges`，例如 `0.5m`、`1.8m` 等近距离值，说明激光传感器本身在正常工作。

### 2.3 检查本地代价图

```bash
ros2 topic echo /robot01/local_costmap/costmap --once
```

输出 `data` 几乎全为 `0`，说明本地代价图没有被激光障碍更新。

### 2.4 关键定位：查看话题订阅关系

```bash
ros2 topic info /robot01/scan -v
```

发现订阅 `/robot01/scan` 的节点只有：

- `amcl`
- `rviz2`

**没有 `local_costmap` 和 `global_costmap`**。

这是本次问题的关键证据：代价图节点根本没订阅激光话题。

## 3. 根因

Nav2 在 `use_namespace=true` 启动时，代价图节点实际运行在**子命名空间**下：

```text
local_costmap 节点命名空间：/robot01/local_costmap
global_costmap 节点命名空间：/robot01/global_costmap
```

而 `nav2_params.yaml` 中观测源使用的是相对话题名：

```yaml
obstacle_layer:
  observation_sources: scan
  scan:
    topic: scan
```

ROS2 的相对话题名会基于节点命名空间解析：

```text
/robot01/local_costmap + scan  ->  /robot01/local_costmap/scan
```

真实激光话题是：

```text
/robot01/scan
```

两者不匹配，所以 `obstacle_layer` 从未收到激光，动态障碍物自然无法进入代价图。

## 4. 修复

将代价图观测源的激光话题改为绝对路径：

```yaml
obstacle_layer:
  observation_sources: scan
  scan:
    topic: /<robot_namespace>/scan
```

生成 `/tmp/amr_nav2_params_{robot_id}.yaml` 时，把 `<robot_namespace>` 替换为 `robot_id`：

- `robot_ws/src/amr_navigation/scripts/start_nav2.py`
- `robot_ws/src/amr_navigation/src/navigation_server.cpp`

最终生成的参数文件里：

```yaml
topic: /robot01/scan
```

## 5. 验证

重新启动 Nav2 后：

```bash
ros2 topic info /robot01/scan -v
```

订阅者应包含：

- `local_costmap`
- `global_costmap`
- `amcl`
- `rviz2`

之后再下发任务，`robot01` 遇到 `robot02` 时会先停车，再重新规划绕行。

## 6. 经验总结

- **相对话题名不等于直觉话题名**：ROS2 中相对话题会按节点实际命名空间解析；Nav2 的 costmap 节点存在额外子命名空间，配置激光/传感器观测源时优先使用绝对话题名。
- **`ros2 topic info <topic> -v` 是定位“数据没到”的第一工具**：能同时看到发布者、订阅者、命名空间和 QoS。
- **动态避障应交给 Nav2 本地层**：`local_costmap + obstacle_layer + 局部控制器 + BT recovery` 是标准方案，不要在外层 `navigation_server` 手动取消/重发目标。
- **`tf2_ros::TransformListener` 默认订阅全局 `/tf`、`/tf_static`**：如果项目采用命名空间 TF，需要显式 remap 或自定义订阅。
- **QoS 必须核对**：本例激光发布为 `RELIABLE`，`amcl/rviz2` 为 `BEST_EFFORT`，虽然本次不是主因，但排查时必须确认订阅关系与 QoS 兼容性。

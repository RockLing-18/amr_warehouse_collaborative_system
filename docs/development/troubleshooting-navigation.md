# 导航地图 / 代价图显示问题排查记录

**状态：** 已解决
**范围：** `amr_navigation` / `amr_description` / `amr_agent` 的 Nav2 + RViz 显示链路
**关键词：** ROS2 namespace、TF 帧、lifecycle、QoS、initialpose 时序、单线程 executor

本文档完整记录「RViz 不显示地图 / 代价图」问题的排查过程。它不只是一个 bug 的修复记录，而是一次**一次症状背后隐藏多个独立根因**的分层排查案例，适合作为面试中的技术亮点。

---

## 1. 背景与现象

启动 `navigation_server` 与 Nav2 后出现（V1 当前默认由 `start_nav2.py` 在独立终端拉起 Nav2，也支持 `launch_nav2:=true` 的 `fork + ros2 launch` 兼容模式）：

1. RViz 地图时有时无。
2. 全局/局部代价图不渲染。
3. 机器人模型不出现在 RViz 地图上。
4. 同一套启动命令，有时正常、有时失败，具有偶发性。

伴随日志反复出现：

```text
Timed out waiting for transform from robot01_base_link to robot01_odom
tf error: Invalid frame ID "robot01_odom" ... frame does not exist

Invalid frame ID "map" passed to canTransform ... frame does not exist

Message Filter dropping message: frame 'robot01_laser_link' ... queue is full

AMCL cannot publish a pose or update the transform. Please set the initial pose...
```

---

## 2. 涉及的关键概念

排查前先明确四层依赖关系，后续每一步都按这四层去验证：

1. **通信层（namespace / topic / QoS）**：节点在哪个 namespace，话题如何解析，QoS 是否兼容。
2. **TF 坐标树**：`map -> odom -> base_footprint -> base_link -> sensor` 是否完整。
3. **生命周期（lifecycle）**：map_server / amcl / costmap 是否走到 `active`。
4. **消息时序**：一次性消息（initialpose）是否在订阅者就绪后才送达。

地图渲染只依赖「Map 话题」；代价图和机器人模型渲染则依赖「TF 树完整 + 代价图节点已激活并发布」。

---

## 3. 排查方法论与工具

核心思路：**分层收窄 + 证据驱动 + 成败日志对比**。

| 手段 | 用途 |
| --- | --- |
| `ros2 topic list` | 看话题是否存在（有发布者或订阅者） |
| `ros2 topic info <topic> -v` | 看发布者/订阅者的 namespace 和 QoS |
| `ros2 topic echo <topic> --once` | 看是否真的有数据在发 |
| `ros2 control list_controllers -c <ns>/controller_manager` | 看控制器是否 active |
| `ros2 lifecycle get <node>` | 看 lifecycle 节点状态 |
| `strings <某个 .so>` | 反查 ROS 库内部硬编码的话题名 / 帧名 |
| 对比成功/失败两次日志 | 定位偶发时序问题 |

两个重要经验：

- **`ros2 topic echo` 有 QoS 假阴性**：默认用 volatile，而某些话题（costmap、diff_drive 的 TF）是 transient_local，匹配不上会一直等不到数据。需用 `--qos-durability transient_local`。
- **RViz 的 TF 订阅者是匿名节点**：在 `ros2 topic info` 里显示为 `transform_listener_impl_xxxx`，而不是 `rviz2`，直接 grep `rviz2` 会误判。

---

## 4. 根因与修复（按排查顺序）

### 4.1 Nav2 没有真正进入 namespace

**现象证据**

RViz 在 `/robot01`，但 `map_server` 把地图发到了 `/map`，两边对不上，地图显示不出来。

**根因**

`robot_ws/src/amr_navigation/launch/navigation2.launch.py` 向 Nav2 的 `bringup_launch.py` 传了 `namespace`，但漏传了 `use_namespace`：

- Nav2 的 `bringup_launch.py` 中，只有 `use_namespace=true` 时才会执行 `PushRosNamespace`。
- 默认 `use_namespace=false`，于是 Nav2 全套节点（map_server、amcl、costmap、lifecycle_manager）都落在根 `/`。
- `navigation_server` 用相对名去找 `lifecycle_manager_navigation/manage_nodes`，解析成 `/robot01/...`，但真实服务在 `/...`，导致生命周期也起不来。

**修复**

```python
# navigation2.launch.py
launch_arguments={
    'map': map_yaml_path,
    'use_sim_time': use_sim_time,
    'params_file': nav2_param_path,
    'autostart': autostart,
    'use_namespace': 'true',   # 关键：补上这一行
    'namespace': namespace}.items(),
```

---

### 4.2 nav2 参数里使用了绝对话题

**根因**

`robot_ws/src/amr_navigation/config/nav2_params.yaml` 中写死了绝对话题，和 namespace 规则冲突：

```yaml
bt_navigator:
  ros__parameters:
    odom_topic: /odom        # 错误：根 /odom
...
local_costmap / global_costmap:
  scan.topic: /scan          # 错误：根 /scan
```

机器人实际发布的是 `/robot01/odom`、`/robot01/scan`。

**修复**

改为相对名，让它们按 namespace 解析：

```yaml
odom_topic: odom
scan.topic: scan
```

---

### 4.3 TF 帧名前缀不一致

**现象证据**

日志里激光帧是 `robot01_laser_link`，但 local_costmap 报 `robot01_base_link -> robot01_odom` 查不到，说明 nav2 期望的帧名和机器人实际发布的帧名不一致。

**根因**

- AMR URDF 所有 TF 帧带 `robot_id + "_"` 前缀：`robot01_base_footprint`、`robot01_base_link`、`robot01_odom`、`robot01_laser_link`。
- `nav2_params.yaml` 却用无前缀帧名：`base_footprint`、`base_link`、`odom`。
- 多机器人共用一个 Gazebo 世界时，TF 帧必须按机器人区分，所以前缀是必要的；nav2 参数必须跟随。

**修复**

1. `nav2_params.yaml` 用 `<robot_prefix>` 占位符：

```yaml
amcl:
  base_frame_id: "<robot_prefix>base_footprint"
  odom_frame_id: "<robot_prefix>odom"
  global_frame_id: "map"        # map 帧保持无前缀，来自 map_server

bt_navigator:
  robot_base_frame: <robot_prefix>base_link

local_costmap:
  global_frame: <robot_prefix>odom
  robot_base_frame: <robot_prefix>base_link

global_costmap:
  global_frame: map
  robot_base_frame: <robot_prefix>base_link
```

2. `navigation_server.cpp` 在拉起 Nav2 前，按 robot_id 生成带前缀的参数文件：

```text
/tmp/amr_nav2_params_robot01.yaml   （<robot_prefix> 被替换为 robot01_）
```

**关键决策**

`map` 帧保持无前缀（它来自 `map_server`，是共享世界坐标）；`odom`、`base_link` 等机器人自身帧才加前缀。AMCL 发布 `map -> robot01_odom`，形成 `map -> robot01_odom -> robot01_base_footprint -> robot01_base_link` 的完整树。

---

### 4.4 diff_drive_controller 的 TF 发布问题（两层）

**现象证据**

`ros2 topic echo /robot01/odom --once` 显示：

```text
frame_id: robot01/robot01_odom
child_frame_id: robot01/robot01_base_footprint
```

帧名被重复加了前缀，且 `/robot01/tf` 的发布者里没有 `amr_diff_drive_controller`。

**根因（两层）**

1. **帧名前缀叠加**：`diff_drive_controller` 默认 `tf_frame_prefix_enable=true`，而 `tf_frame_prefix` 为空时，会用控制器的 **namespace** 作为前缀。于是 `odom_frame_id: robot01_odom` 被拼成 `robot01/robot01_odom`。
2. **TF 话题是绝对 `/tf`**：用 `strings /opt/ros/humble/lib/libdiff_drive_controller.so` 可以看到它硬编码 `/tf`，导致 TF 发到根，而不是 `/robot01/tf`。

**修复**

1. `robot_ws/src/amr_description/config/amr_ros2_controller.yaml`：

```yaml
amr_diff_drive_controller:
  ros__parameters:
    enable_odom_tf: true
    tf_frame_prefix_enable: false   # 禁止 namespace 前缀叠加
```

2. `robot_ws/src/amr_description/urdf/amr.ros2_control.xacro` 的 gazebo 插件：

```xml
<remapping>amr_diff_drive_controller/cmd_vel_unstamped:=cmd_vel</remapping>
<remapping>/tf:=tf</remapping>
```

修复后 `/robot01/odom` 的 frame 变为 `robot01_odom -> robot01_base_footprint`，且 `/robot01/tf` 出现控制器这个发布者。

---

### 4.5 robot_state_publisher / RViz 的 TF 话题

**现象证据**

`/robot01/tf_static` 的 `Publisher count: 0`，但 `robot_state_publisher` 节点明明在运行。`strings /opt/ros/humble/lib/librobot_state_publisher_node.so` 显示它硬编码 `/tf`、`/tf_static`。

**根因**

`robot_state_publisher` 把静态 TF（`base_footprint -> base_link -> laser_link`）发到了根 `/tf_static`，而 Nav2 订阅的是 `/robot01/tf_static`，所以 `robot01_base_link` 一直不存在，costmap 无法激活。

**修复**

`robot_ws/src/amr_description/launch/spawn_amr.launch.py` 给 RSP 加：

```python
remappings=[('/tf', 'tf'), ('/tf_static', 'tf_static')]
```

RViz 的 TF 显示同理，`navigation2.launch.py` 中给 rviz2 节点加相同 remappings。

---

### 4.6 initialpose 下发时序竞态（重点，偶发失败的根因）

#### 现象

成功与失败两份日志对比：

- 成功：AMCL 先完成 `initPubSub`（订阅就绪），`navigation_server` 再发 `initialpose`，AMCL 打印 `initialPoseReceived`。
- 失败：AMCL 还在 `Configuring`，`navigation_server` 已经把 `initialpose`（volatile、只发一次）发出去了，AMCL 还没订阅，消息丢失。之后 AMCL 反复打印 `AMCL cannot publish a pose ... Please set the initial pose`，`map` 帧永远建立不起来，代价图无法激活。

#### 根因

AMCL 的 `initialpose` 订阅在 `on_configure` 创建，但只有 lifecycle 走到 `active` 后回调才真正生效。`navigation_server` 在发出 STARTUP 后立刻发位姿，存在「发早于订」的竞态。

#### 修复迭代（三次）

**迭代 1：周期重发**

在 `WAITING_FOR_LOCALIZATION` 阶段以 1Hz 重发 3 次。能用，但依赖“次数”，且 AMCL 会收到多次相同位姿，不够确定。

**迭代 2：轮询 AMCL 的 `get_state` 服务，等 active 再发**

实现后发现一个更隐蔽的问题：`async_send_request + future.wait_for()` 在**单线程 executor** 的定时器回调里同步等待，executor 被阻塞后无法处理服务响应回调，于是每次都 500ms 超时返回 false，位姿永远发不出去。

**迭代 3（最终方案）：订阅 AMCL 的 `transition_event`**

纯异步、无阻塞、确定性强：

```cpp
// navigation_server.cpp
m_amcl_transition_sub = this->create_subscription<TransitionEvent>(
    "amcl/transition_event", 10,
    [this](const TransitionEvent::SharedPtr msg) {
        if(msg && msg->goal_state.id == lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE) {
            m_amcl_active = true;
            publishInitialPoseIfReady();   // AMCL active 后才真正发布
        }
    });
```

并且把「等 AMCL active」的检查收口到 `publishInitialPoseIfReady()` 内部，保证订阅回调和状态机 tick 两条发布路径都受控。

#### 顺带修复的两个问题

1. **`use_sim_time` 重复声明异常**：`use_sim_time` 是 rclcpp 自动声明的特殊参数，手写 `declare_parameter("use_sim_time", true)` 会抛 `ParameterAlreadyDeclaredException`。改为先 `has_parameter` 再 `set_parameter`。
2. **initialpose 时间戳**：让 `navigation_server` 使用仿真时间，保证 initialpose 的 header 时间戳与 TF/AMCL 一致，消除 `Lookup would require extrapolation into the future`。

---

## 5. 日志解读与常见告警

以下告警在**启动瞬间**出现，属于正常现象，定位建立后自动消失：

| 日志 | 含义 | 是否需要处理 |
| --- | --- | --- |
| `Invalid frame ID "map" ... frame does not exist` | AMCL 尚未建立 `map -> odom` 前，costmap/bt_navigator 提前查 `map` 帧 | 否，定位后消失 |
| `Message Filter dropping ... earlier than all data in transform cache` | TF 缓存尚未就绪时收到激光/里程计消息 | 否，TF 树稳定后消失 |
| `No transition matching 1 found for current state active` | lifecycle manager 重复 bringup 时的状态冲突 | 否，非致命 |
| RViz `active samplers with a different type ...` | OpenGL 纹理告警 | 否，不影响渲染 |

判断标准：**如果定位成功后这些告警仍持续刷屏，才需要进一步排查时间同步问题。**

---

## 6. 排查命令速查表

```bash
# 看 TF 发布者/订阅者与 QoS
ros2 topic info /robot01/tf -v
ros2 topic info /robot01/tf_static -v

# 看里程计实际帧名（定位双重前缀）
ros2 topic echo /robot01/odom --once

# 看控制器状态
ros2 control list_controllers -c /robot01/controller_manager

# 看静态 TF 内容（确认 base_footprint -> base_link 存在）
ros2 topic echo /robot01/tf_static --once --qos-durability transient_local

# 看代价图是否在发（注意 transient_local）
ros2 topic echo /robot01/global_costmap/costmap --once --qos-durability transient_local

# 看 AMCL 是否已定位
ros2 topic echo /robot01/amcl_pose --once

# 反查 ROS 库内部硬编码的话题名
strings /opt/ros/humble/lib/libdiff_drive_controller.so | grep -E '^/tf$'
strings /opt/ros/humble/lib/librobot_state_publisher_node.so | grep -E '/tf'
```

---

## 7. 最终验证结果

- 地图、全局/局部代价图、机器人模型均能在 RViz 正确显示。
- `map -> robot01_odom -> robot01_base_footprint -> robot01_base_link -> robot01_laser_link` TF 树完整。
- 两个 AMR（robot01 / robot02）可分别通过 `/robot01/cmd_vel`、`/robot02/cmd_vel`（`geometry_msgs/msg/Twist`）独立控制移动，证明 topic / namespace 隔离正确。

---

## 8. 面试要点

1. **不是修一个 bug，而是分层定位**：通信边界（namespace/topic/QoS）→ TF 树 → lifecycle → 消息时序，每层独立验证。
2. **根因思维**：`map frame does not exist` 只是表象，真正链路是 odom 帧缺失 → 控制器 TF 话题/前缀错误 → initialpose 时序竞态，而不是在报错处打补丁。
3. **偶发问题定位**：成功/失败两份日志对比，锁定时序竞态。
4. **ROS2 机制理解**：namespace 与 remapping、TF 帧约定、lifecycle 状态机、QoS、`use_sim_time`、单线程 executor 下同步服务调用会死锁。
5. **工具链**：`ros2 topic info -v`、`ros2 topic echo`、`ros2 control list_controllers`、`strings` 反查二进制、日志对比。

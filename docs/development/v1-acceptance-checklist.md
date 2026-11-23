# V1 验收与归档清单

**状态：** Draft
**范围：** Edge Server、AMR（`amr_agent` / `amr_behavior_manager` / `amr_navigation`）、Simulation、WebSocket
**对应文档：** `docs/requirements/srs.md` 第 7 章验收标准

## 1. 目的

V1 归档前，按本清单逐项验证核心业务闭环，并保留可复现证据。

## 2. 前置准备

### 2.1 构建

```bash
# Edge Server
cd /home/zyr/code/amr_warehouse_collaborative_system
cmake -S edge_server -B edge_server/build
cmake --build edge_server/build -j2

# robot_ws
source /opt/ros/humble/setup.bash
cd /home/zyr/code/amr_warehouse_collaborative_system/robot_ws
colcon build --merge-install --symlink-install

# simulation_ws
cd /home/zyr/code/amr_warehouse_collaborative_system/simulation_ws
colcon build --merge-install --symlink-install

# tool_ws
cd /home/zyr/code/amr_warehouse_collaborative_system/tool/tool_ws
colcon build --merge-install --symlink-install
```

### 2.2 启动顺序

```bash
# 1. EMQX
emqx start

# 2. Edge Server
./edge_server/bin/edge_server

# 3. Gazebo + Simulation Manager
source /opt/ros/humble/setup.bash
source .../simulation_ws/install/setup.bash
ros2 launch warehouse_simulation warehouse.launch.py

# 4. AMR 三件套（统一入口，勿重复手动 ros2 run）
source .../robot_ws/install/setup.bash
ros2 launch amr_bringup amr_bringup.launch.py \
  robot_id:=robot01 \
  use_sim_time:=true \
  separate_terminals:=true

# 5. navigation_server 收到 MapInfo 后自动弹 Nav2 终端
```

### 2.3 地图准备

```bash
# 上传并激活地图（warehouse_id / version 按实际填）
curl -X POST "http://127.0.0.1:8080/api/maps/upload?warehouse_id=warehouse_01&version=2.0&activate=true" \
  -F "file=@/path/to/warehouse_01.zip"

# 查询当前生效地图
curl "http://127.0.0.1:8080/api/maps/active?warehouse_id=warehouse_01"
```

## 3. 验收项

### AC-001 构建

- [ok] Edge Server 构建通过。
- [ok] `robot_ws` / `simulation_ws` / `tool_ws` 构建通过。
- 证据：终端构建输出截图或日志。

### AC-002 Bootstrap

- [ok] AMR 启动后能通过 HTTP 获取 `warehouse_id` 和 MQTT 配置。
- 证据：`amr_agent` 日志 `bootstrap success`。

### AC-003 注册

- [ok] AMR 通过 MQTT 完成注册。
- [ok] Edge 能识别 `robot_id`，注册响应包含 `initial_pose`。
- 证据：Edge 日志 `robot register`，AMR 日志 `receive register response`。

### AC-004 / AC-005 地图管理与同步

- [ok] 地图包可上传、下载、查询、激活。
- [ok] 本地地图缺失或版本不一致时，AMR 自动下载并解压。
- [ok] 下载后关键文件校验通过（`map.yaml` / 地图图片 / `zones.yaml` / `stations.yaml`）。
- 证据：`map_service` 日志、`map_manager` 日志。

### AC-006 导航初始化

- [ok] `navigation_server` 收到 `MapInfo` 后自动拉起 Nav2 终端。
- [ok] AMCL 激活，`initialpose` 下发，最终打印 `navigation ready`。
- [ok] `amr/robot01/status` 上报 `pose` 接近 `initial_pose`，不再全 0。
- 证据：`navigation_server` 日志、MQTT 状态消息。

### AC-007 / AC-008 任务注入与下发

```bash
curl -X POST http://127.0.0.1:8080/api/tasks \
  -H "Content-Type: application/json" \
  -d '{"task_id":"task_v1_unload_10","task_type":"UNLOAD","robot_ids":["robot01"],"goods":[{"goods_id":"A","goods_name":"A","quantity":10}]}'
```

- [ok] 返回 `code=0`，任务 `PENDING`。
- [ok] Edge 向 `amr/robot01/task` 下发 `TASK_ASSIGN`。
- [ok] AMR 上报 `TASK_ACCEPTED`，任务进入 `ACCEPTED`。
- [ok] 货物申请获批后任务进入 `RUNNING`。
- 证据：HTTP 响应、MQTT 日志。

### AC-009 / AC-010 正常任务执行与结果反馈

- [ok] `UNLOAD`：先到 `UNLOAD` 取货点，再到 `STORAGE` 放货点。
- [ok] `LOAD`：先到 `STORAGE` 取货点，再到 `LOAD` 放货点。
- [ok] 单货物 `quantity=10` 能 `SUCCEEDED`。
- [ok] 单货物 `quantity=20` 能按 `carry_capacity=10` 分两趟完成。
- [ok] 完成后 AMR 返回充电点，状态回 `IDLE`。
- [ok] Edge 查询结果正确：

```bash
curl http://127.0.0.1:8080/api/tasks/task_v1_unload_10
```

- 证据：`amr_behavior_manager` 日志、`amr_agent` MQTT 日志、`navigation_server` 导航日志、任务查询 JSON。

### AC-011 任务取消

```bash
curl -X POST http://127.0.0.1:8080/api/tasks/task_v1_unload_10/cancel
```

- [ok] 任务进入 `CANCELLED`。
- [ok] AMR 取消当前导航，有在途申请则回退 `in_progress`。
- [ok] 不覆盖已完成的终态任务。
- 证据：HTTP 响应、行为日志、任务查询 JSON。

### AC-012 / AC-013 / AC-014 断网续行与结果补报

**状态：Planned —— 本轮不测试断网续行与结果补报，后续单独验证。**

- [ok] 停止 Edge Server 进程：AMR 完成当前已获批搬运后停止下一行动。
- [ok] 停止 Edge-side EMQX：AMR 检测断连并停止后续申请。
- [ok] 断网期间产生的 `CARRY_COMPLETED` 写入 `pending_task_reports.json`。
- [ok] 恢复后 AMR 重传，收到 `status/ack` 后清理本地记录。
- [ok] 重复消息不重复计数、不覆盖终态。
- 证据：`amr_agent` 日志、`pending_task_reports.json` 文件、Edge 任务状态。

### AC-015 / AC-016 / AC-017 仿真与多 AMR

- [ok] WebSocket `robot_list` 能推送机器人列表。
- [ok] 两个 AMR 用不同 `robot_id` / `instance_id` 运行，命名空间和 TF 前缀隔离。
- [ok] 关键失败场景有可识别日志。
- 证据：Edge WebSocket 日志、Simulation Manager 日志、`ros2 node list`。

### AC-018 文档完整性

- [ok] 需求、架构、构建、接口、验收文档齐全。
- [ok] SRS 7.2 状态矩阵与实际实现一致。

## 4. 核心业务闭环用例

### 用例 1：UNLOAD 单趟

```bash
curl -X POST http://127.0.0.1:8080/api/tasks \
  -H "Content-Type: application/json" \
  -d '{"task_id":"task_v1_unload_10","task_type":"UNLOAD","robot_ids":["robot01"],"goods":[{"goods_id":"A","goods_name":"A","quantity":10}]}'
```

预期：`PENDING -> ACCEPTED -> RUNNING -> SUCCEEDED`，AMR 返回充电点。

### 用例 2：UNLOAD 多趟

`quantity=20`，`carry_capacity=10`。

预期：两次货物申请，两次 `CARRY_COMPLETED`，最终 `completed_quantity=20`。

### 用例 3：LOAD 单趟

`task_type=LOAD`，`goods_id=A`，`quantity=10`。

预期：`STORAGE -> LOAD` 路线，任务 `SUCCEEDED`。

## 5. 异常用例

- 导航失败：人为在路径放动态障碍，AMR 上报 `CARRY_ABORTED/NAVIGATION_FAILED`。
- 取消执行中任务：确认取消流程和状态一致性。
- 断 Edge / 断 Broker：确认续行、补报、ACK 清理。
- 重复 `task_id`：不重复创建。
- 重复状态消息：不重复计数、不覆盖终态。

## 6. 归档证据清单

每个验收项至少保留：

- 构建日志或截图。
- 关键节点日志片段（`amr_agent` / `amr_behavior_manager` / `navigation_server` / `edge_server`）。
- HTTP 请求与响应。
- MQTT 消息日志。
- `GET /api/tasks/{task_id}` 终态 JSON。
- RViz / Gazebo 截图或录屏。

## 7. V1 已知边界

- 同一趟搬运暂按单类货物执行，多货物按类型拆分为多趟。
- 地图完整性为文件存在性校验，未做 checksum/size 校验。
- 无交通路权协调与电量管理（V2 范围；V2 路权逻辑内嵌于 `amr_behavior_manager`），多机仅验证身份隔离。
- `FAILED` 状态 V1 不主动使用。

## 8. 状态矩阵

实现状态以 `docs/requirements/srs.md` 7.2 为准；归档前需再次核对。

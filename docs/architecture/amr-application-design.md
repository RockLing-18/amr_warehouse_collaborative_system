# AMR Application 设计

**Status:** Draft

## 1. 目的

本文档定义 AMR 侧模块如何协作完成 V1 任务闭环。它是后续实现 `amr_agent`、`amr_behavior_manager`、`amr_navigation` 的直接依据。

## 2. 设计原则

1. `amr_agent` 只处理 Edge 通信和本地待补报文件。
2. `amr_behavior_manager` 只处理任务状态机和行为编排。
3. `amr_navigation` 只处理 Nav2 导航。
4. AMR 内部模块通过 ROS2 通信，不直接调用对方类实现。
5. MQTT 消息只在 `amr_agent` 出现，业务模块不直接依赖 MQTT。
6. 所有 ROS2 名称统一从 `amr_common` 获取。

## 3. 模块职责

### 3.1 amr_agent

目标职责：

- Bootstrap
- 注册 Edge
- 订阅 Edge 任务、任务共享状态、Edge 状态
- 订阅 AMR 内部状态/事件，并发布到 MQTT
- 维护 `pending_task_reports.json`
- 收到 Edge ACK 后清理待补报记录
- 检测 Edge 状态并进入安全等待

当前代码：

- Bootstrap 和 MQTT 连接已有骨架。
- 任务、状态、待补报逻辑尚未接入。

### 3.2 amr_behavior_manager

目标职责：

- 维护 AMR 任务状态机
- 根据任务和共享状态决定下一步
- 申请货物
- 驱动导航
- 模拟抓取/放置
- 触发完成/放弃上报
- 发布机器人状态和任务状态

当前代码：

- 已有 `NavigationClient` 和 `RobotStatus` 发布骨架。
- 没有任务状态机和货物申请逻辑。

### 3.3 amr_navigation

目标职责：

- 提供 `amr_navigate_to_pose` Action
- 提供 `get_robot_pose` Service
- 发布 `robot_pose`
- 封装 Nav2 Action 调用

当前代码：

- 已有 Action Server、位姿服务和 Nav2 client 基础实现。

## 4. MQTT Topic 处理

### 4.1 amr_agent 订阅

| Topic | 触发动作 |
| --- | --- |
| `edge_server/status` | 更新 Edge 在线状态；异常时安全停止。 |
| `amr/register/response/{robot_id}` | 处理注册响应和地图更新。 |
| `amr/{robot_id}/task` | 接收任务下发或取消。 |
| `warehouse/{warehouse_id}/task/{task_id}/state` | 接收任务共享状态。 |
| `amr/{robot_id}/task/goods/response` | 接收货物申请结果。 |
| `amr/{robot_id}/task/status/ack` | 清理本地待补报记录。 |

### 4.2 amr_agent 发布

| Topic | 触发来源 |
| --- | --- |
| `amr/register/request` | AMR 启动注册。 |
| `amr/{robot_id}/status` | AMR 运行状态和心跳。 |
| `amr/{robot_id}/will` | MQTT Last Will。 |
| `amr/{robot_id}/task/goods/request` | `amr_behavior_manager` 发起货物申请。 |
| `amr/{robot_id}/task/status` | AMR 状态、完成、放弃上报。 |

## 5. AMR 内部接口

当前已存在：

| 类型 | 名称 | 说明 |
| --- | --- | --- |
| Topic | `map_info` | 当前地图信息。 |
| Topic | `robot_status` | 机器人聚合状态。 |
| Topic | `task_status` | 当前任务快照。 |
| Topic | `robot_pose` | 当前位姿。 |
| Action | `amr_navigate_to_pose` | 单点导航。 |
| Service | `get_robot_pose` | 查询位姿。 |

待补充：

| 类型 | 建议名称 | 用途 |
| --- | --- | --- |
| Topic | `task_command` | `amr_agent` 向 `amr_behavior_manager` 下发任务/取消/共享状态。 |
| Topic | `task_event` | `amr_behavior_manager` 向 `amr_agent` 上报申请、完成、放弃、状态。 |

具体消息类型需要进一步确认，见本文末尾问题。

## 6. 正常任务流程

1. `amr_agent` 收到 `amr/{robot_id}/task`。
2. `amr_agent` 解码并转发给 `amr_behavior_manager`。
3. `amr_behavior_manager` 进入 `ACCEPTED`，上报任务接受。
4. `amr_behavior_manager` 读取共享状态，选择 `pending_quantity > 0` 的货物。
5. `amr_agent` 向 Edge 发起货物申请。
6. Edge 批准后，`amr_agent` 收到 `goods/response`。
7. `amr_behavior_manager` 进入 `GRASPING`，模拟抓取完成。
8. `amr_behavior_manager` 根据 `goods_id + task_type` 获取目标点。
9. `amr_behavior_manager` 调用 `amr_navigate_to_pose`。
10. 到达后进入 `PLACING`，模拟放置完成。
11. 如果仍有待搬运货物，返回第 4 步；否则完成当前任务。
12. `amr_agent` 上报 `CARRY_COMPLETED`。

## 7. 取消流程

1. `amr_agent` 收到 `amr/{robot_id}/task` 中的取消指令。
2. 转发给 `amr_behavior_manager`。
3. `amr_behavior_manager` 取消当前导航。
4. 已获批但未完成的数量通过 `CARRY_ABORTED` 上报。
5. 任务状态进入 `CANCELLED`。

## 8. Edge 故障流程

1. `amr_agent` 检测 `edge_server/status` 或 MQTT 连接异常。
2. 如果 AMR 正在搬运已获批货物，允许完成当前次搬运。
3. 完成或放弃结果写入 `pending_task_reports.json`。
4. 不再发起新的货物申请。
5. Edge 恢复后，`amr_agent` 重传待补报结果。
6. Edge 返回 ACK，AMR 删除对应记录。

## 9. 关键实现细节

### 9.1 待补报文件

路径：

```text
/opt/amr/data/amr_runtime/pending_task_reports.json
```

写入：

- 先写临时文件，再原子 rename。
- 按 `message_id` 去重。

清理：

- 收到 `amr/{robot_id}/task/status/ack` 后删除对应 `message_id`。

### 9.2 Edge 状态检测

- 订阅 retained `edge_server/status`。
- 同时检测本地 MQTT 连接状态。
- Edge 状态消息超时或连接断开，都视为 Edge 异常。

### 9.3 货物点位映射

V1 暂不在任务消息中下发坐标，由：

```text
goods_id + task_type
```

映射到：

- 装货区
- 卸载区
- 存储区

已确认关系：

```text
map -> zone -> station -> location
```

文件路径：

```text
<warehouse_dir>/zones.yaml
<warehouse_dir>/stations.yaml
```

`location.type` 使用 `WORK` / `WAIT` / `IDLE`：`WORK` 为作业点、`WAIT` 为等待点、`IDLE` 为充电点。

`location.pose.yaw` 需要支持编辑，不再固定为 0。

货物与区域的关系由 `zones.yaml` 中的 `goods_ids` 维护：

```yaml
zones:
  - id: "unload_zone_A"
    name: "A 卸货区"
    type: "UNLOAD"
    goods_ids: ["A"]
    polygon: [...]
```

规则：

- `UNLOAD` 任务：在 `type=UNLOAD` 且 `goods_ids` 包含目标货物的 zone 中找目标 station/location。
- `LOAD` 任务：在 `type=LOAD` 且 `goods_ids` 包含目标货物的 zone 中找目标 station/location。
- 任务消息不携带 `target_zone_id`、`target_station_id`、`target_location_id`。
- V1 通过 `zone.type` 区分货物区域与充电区域（`IDLE`）；其余 zone 默认货物相关，后续如出现其他非货物区域，再增加字段区分。

### 9.4 心跳

AMR 每 5 秒通过 `amr/{robot_id}/task/status` 上报一次 `STATUS`。

## 10. 已确认决策

1. `amr_agent` 与 `amr_behavior_manager` 使用 ROS2 Topic 通信。
2. 新增 `amr_interfaces/msg/TaskCommand.msg`、`TaskEvent.msg`、`TaskGoods.msg`、`TaskGoodsStatus.msg`。
3. 货物申请由 `amr_behavior_manager` 根据 Edge 共享状态决定，`amr_agent` 负责发 MQTT 请求。
4. 货物点位映射文件由 `amr_behavior_manager` 加载。
5. `GRASPING` 和 `PLACING` 在 V1 使用固定延时或状态直接推进，不接机械臂。
6. `amr_behavior_manager` 发布内部 `task_status`，`amr_agent` 翻译成 MQTT `amr/{robot_id}/task/status`。
7. 导航失败时 V1 不自动重试，直接上报 `CARRY_ABORTED`，原因 `NAVIGATION_FAILED`。

## 11. 新增 ROS2 消息

### 11.1 TaskGoods

```text
string goods_id
string goods_name
int32 total_quantity
```

### 11.2 TaskGoodsStatus

```text
string goods_id
string goods_name
int32 total_quantity
int32 in_progress_quantity
int32 completed_quantity
int32 pending_quantity
```

### 11.3 TaskCommand

`amr_agent -> amr_behavior_manager`

```text
string command_type
string task_id
string warehouse_id
string task_type
string[] robot_ids
TaskGoods[] goods
TaskGoodsStatus[] goods_status
string state
string reason
builtin_interfaces/Time stamp
```

`command_type`：

- `TASK_ASSIGN`
- `TASK_CANCEL`
- `TASK_STATE_UPDATE`

### 11.4 TaskEvent

`amr_behavior_manager -> amr_agent`

```text
string event_type
string task_id
string warehouse_id
string task_state
TaskGoods[] goods
string request_id
string reason
builtin_interfaces/Time stamp
```

`event_type`：

- `TASK_ACCEPTED`
- `GOODS_REQUEST`
- `CARRY_COMPLETED`
- `CARRY_ABORTED`
- `STATUS`

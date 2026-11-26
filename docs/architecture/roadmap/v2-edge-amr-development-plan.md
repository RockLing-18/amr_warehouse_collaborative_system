# V2 Edge Server / AMR 开发计划

**状态：** Planned
**日期：** 2026-11-24
**前置文档：** `docs/architecture/roadmap/v2-resource-arbitration-and-bt.md`、`v2-implementation-spec.md`、`v2-traffic-control-chain.md`、`docs/architecture/adr/0002-resource-arbitration-and-bt-layering.md`

## 1. 范围

本计划只覆盖 V2 的 Edge Server 与 AMR 两大块，不含 Cloud / Web / 机械臂。目标：

- Edge：统一资源仲裁（`ResourceBroker` + `traffic_rights_service` + `tbl_resource_lease`）、充电点 lease、battery 只读镜像与关键电量日志。
- AMR：`TrafficBridge` 透明桥、`TrafficGate` 路权状态机、`traffic_gate_loader`、`amr_power_manager`、行为树骨架、`PowerMode` 发布。

## 2. 前置阻塞（必须先修）

以下 4 个 `amr_navigation` 缺陷不改，路权的 `cancel → 等待点 → 重发 D` 会被放大：

| # | 问题 | 文件 |
| --- | --- | --- |
| 1 | 无条件 `ACCEPT_AND_EXECUTE` + supersede | `navigation_server.cpp` |
| 2 | 60 s 超时只 abort、不 cancel Nav2 goal | `navigation_server.cpp` |
| 3 | `m_nav_epoch` 非 atomic / 无锁竞态 | `navigation_server.cpp` |
| 4 | `accepted` 语义应改为 `submitted` | `navigation_manager.cpp` |

完成标准：V1 的 AC-009 / AC-011 回归通过。

## 3. 接口与数据模型先行

### 3.1 `amr_common` / `common/mqtt`

- 新增 ROS2 topic 常量：`TRAFFIC_REQUEST`、`TRAFFIC_RESPONSE`、`POWER_MODE`、`POWER_LEVEL`。
- 替换 MQTT 常量：`AMR_TRAFFIC_REQUEST`、`AMR_TRAFFIC_RESPONSE`、`AMR_TRAFFIC_SUBSCRIBE`，并补 `makeAmrTrafficRequestTopic()` / `makeAmrTrafficResponseTopic()`。

### 3.2 `amr_interfaces`

- 新增消息：`TrafficRequest.msg`、`TrafficResponse.msg`、`PowerMode.msg`、`BatteryState.msg`。
- `MapInfo.msg` 新增 `string traffic_gates_yaml`。

### 3.3 SQLite

- 新增 `tbl_resource_lease`：
  - 部分唯一索引仅覆盖 `GRANTED` / `HELD`；
  - `PENDING` 可共存；
  - `queue_seq` 单调递增；
  - `lease_ttl` 默认 60 s，自 `GRANTED` 起算；
  - `PENDING` 超时 60 s。
- 新增 `tbl_robot_battery_log`，仅记录关键事件：`LOW_ENTERED` / `LOW_CLEARED` / `FULL` / `CHARGE_STARTED` / `CHARGE_STOPPED`。

## 4. Edge Server 开发计划

| 阶段 | 内容 | 交付物 |
| --- | --- | --- |
| E1 | 启用 `amr/+/traffic_rights/request` 订阅，按 payload `op` 分发，`robot_id` 与 topic 交叉校验 | `amr_communication.cpp` |
| E2 | `traffic_rights_service`：`REQUEST` / `CANCEL` / `RELEASE` 处理、`request_id` 幂等、`epoch` 防陈旧、单播响应 | `traffic_rights_service.{h,cpp}` |
| E3 | `ResourceBroker`：FIFO `queue_seq`、原子授权、`PENDING` / `GRANTED` / `HELD` / `RELEASED` / `EXPIRED` / `CANCELLED`、`lease_ttl` 回收、重启恢复 | `resource_broker.{h,cpp}` |
| E4 | `IdlePointAllocator` 升级为 lease，充电点占用可回收、可恢复 | `idle_point_allocator.{h,cpp}` |
| E5 | Edge 配置：`PENDING` 超时、`lease_ttl`、电量日志开关 | `edge_server.yaml` |
| E6 | 脚本/日志验证：两车争同一资源只有一个获批；显式 cancel 两态；TTL 回收；重启恢复 | 验证记录 |

## 5. AMR 开发计划

| 阶段 | 内容 | 交付物 |
| --- | --- | --- |
| A1 | 补 `TrafficBridge`：`traffic_request` ↔ MQTT，MQTT response ↔ `traffic_response`，照抄 `TaskBridge` 范式 | `amr_agent/src/bridge/traffic_bridge.{h,cpp}` |
| A2 | `traffic_gate_loader`：读 `traffic_gates.yaml`，派生 `long_axis` / `centroid` | `amr_behavior_manager/src/traffic_gate_loader.{h,cpp}` |
| A3 | `TrafficGate`：中心点 `inside` 边沿 + 滞回 + `entry_side` 锁存 + 同侧就近等待点 + 四触发释放 | `amr_behavior_manager/src/traffic_gate.{h,cpp}` |
| A4 | 填实 `amr_behavior_manager_node.cpp` 的 feedback 空回调，接 `TrafficGate` | `amr_behavior_manager_node.cpp` |
| A5 | `MapInfo.traffic_gates_yaml` 下发链路：`map_manager.cpp` 填充 → behavior 回调 → loader | `map_manager.cpp` / behavior 回调 |
| A6 | `amr_power_manager`：订阅 `PowerMode`，按仿真时钟速率积分，发布 `BatteryState`，clamp + 滞回 + `mode_timeout_sec` 回落 | 新 ROS2 包 |
| A7 | `PowerMode` 发布：行为层 1 Hz 定时器内发布 IDLE/WORK/CHARGING；移除两处 `battery=100.0` 硬编码 | `amr_behavior_manager_node.cpp` / `amr_agent_node.cpp` |
| A8 | 行为树：BT.CPP v4 + Groot2，XML 放 `config/behavior_trees/`，实现 `RequestOnApproach` / `WaitAndRetry` / `ReleaseTrafficRight` / `BatteryOK` 等节点 | `amr_behavior_manager/src/bt_nodes/` |
| A9 | 电量上报接 Edge：`robot_status.battery` 改为 `BatteryState` 值，`amr_agent` 只读转发 | `amr_agent_node.cpp` |

## 6. 联调验收

- 两车同时申请同一 `resource_id`，仅一台获批。
- 持权 AMR 掉线 → `lease_ttl` 到期自动回收。
- Edge 重启 → 占用状态正确重建。
- AMR 进区 `false→true` 申请、`REQUESTING` 收 `GRANTED` 零动作。
- AMR 被拒 → cancel → 等待点 → 固定 10 s 幂等重发 → 获批立即重发 D。
- AMR 出区 `true→false` 释放，改道由出区边沿或 TTL 承接。
- `battery` 不再恒为 100；低电量回充；Edge 只读镜像不修改 SOC。
- `task_state` 全程不变；`task_command` / `task_event` 零改动。
- `ros2 node info` 确认无 `plan` 订阅、`amr_power_manager` 无 `cmd_vel` 订阅。

## 7. 推荐实施顺序

1. 修复 `amr_navigation` 4 个阻塞缺陷。
2. 完成 `amr_common` / `amr_interfaces` / SQLite 数据层。
3. Edge：E1 → E2 → E3 → E4 → E6。
4. AMR 桥与几何：A1 → A2 → A5 → A3 → A4。
5. AMR 电量：A6 → A7 → A9。
6. AMR 行为树：A8。
7. Edge + AMR 联调，按 §6 逐项验收。

## 8. 风险

- BT.CPP v4 在 ROS2 Humble 无官方 rosdep 包，需源码构建，先验证依赖再写 BT 节点。
- `lease_ttl` 固定 60 s 可能对长窄道偏短，V2 联调时再按实际通过时间调整。
- 等待点允许区内/区外后，runtime `entry_side` 与 editor 归档必须用同一 `s` 公式。

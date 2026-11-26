# ADR-0002: V2 资源仲裁与行为树分层

## 状态

Accepted

## 日期

2026-11-24

## 背景

V2 引入交通路权、电量真实化和行为树执行骨架。三类能力共享同一套「申请—批准—占用—释放」语义，需要明确统一仲裁层、模块归属、地图数据模型、行为树技术选型与急停边界，避免实现者做出多套不一致的架构。

## 决策

1. 统一资源仲裁由 Edge 侧 `ResourceBroker` + `traffic_rights_service` 承担；AMR 侧不持有全局 lease、不做排队排序。
2. 路权逻辑不设独立 ROS2 包，实现为 `amr_behavior_manager` 包内 `TrafficGate`；`amr_agent` 只补 `TrafficBridge` 透明桥。
3. 路权使用专用 ROS2 topic `traffic_request` / `traffic_response`，不复用 `task_command` / `task_event`。
4. MQTT 使用 `amr/{robot_id}/traffic_rights/request` 与 `amr/{robot_id}/traffic_rights/response`；动作由 payload `op` 承载，不采用动作分层 topic。
5. 窄道几何使用独立、可选的地图第 4 文件 `traffic_gates.yaml`；缺失表示当前地图无窄道区域；`metadata.yaml` 键名使用 `traffic_gate`。
6. `traffic_gates.yaml` 每条记录为 `resource_id` + 4 点凸 `polygon`（点序 `P1→P2` 端边 L1 / `P2→P3` 长轴 / `P3→P4` 端边 L2）+ `wait_poses{l1,l2}`（每侧 ≥1）。
7. 等待点位置由人工录入，允许在区域内 / 区域外，但须位于真实窄道上游；工具按 `s=dot(p-centroid, long_axis)` 自动归档到 `l1` / `l2`，`yaw` 由长轴派生、不人工填写。
8. 路权主判据为 AMR 中心点 `point_in_poly(center, polygon)` 的 `inside` 进出边沿；全程不订阅 Nav2 `plan`；提前量由拉长控制区 `L_pull` 提供。
9. 进入侧由 `s=dot(enter_pose-centroid, long_axis)` 于进区边沿计算一次并在本次穿越周期锁存。
10. 释放四触发取最早：出区边沿 / 任务结束 / `lease_ttl` 到期 / 显式 `CANCEL`；改道释放由出区边沿或 TTL 承接，不设独立检测机制。
11. Edge 新建 `tbl_resource_lease`；`lease_ttl` 自 Edge 授权 `GRANTED` 时刻起算，默认 60 s，无静默续租。
12. 行为树采用 BehaviorTree.CPP v4 + Groot2；XML 放 `amr_behavior_manager/config/behavior_trees/`。
13. 电量采用 B 方案（本地速率积分）+ D 方案（`PowerMode` 单向声明档位，`amr_power_manager` 零判定）。
14. V2 不新增独立软件急停节点 / 接口；急停语义由任务取消 / 异常路径的 `cancelNavigation()` + `cmd_vel` 归零承接。

## 后果

- AMR 侧只新增 `TrafficGate`、`TrafficBridge`、`traffic_gate_loader` 与 `amr_power_manager`，不新增独立交通模块。
- 地图包校验、`MapInfo.traffic_gates_yaml` 下发、`traffic_gates_editor` 两阶段录入需保持一致。
- BT.CPP v4 在 ROS2 Humble 下无官方 rosdep 包，需要明确源码构建依赖方式。

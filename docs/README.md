# 项目文档索引

## 文档状态标注

- `Implemented`：代码中已存在并可验证。
- `Partial`：已有骨架或部分实现，尚未完整满足需求。
- `Planned`：目标架构或后续版本规划，当前未实现。

| 文档 | 当前状态 |
| --- | --- |
| SRS | Draft |
| System Architecture | Target Architecture |
| AMR Internal Architecture | Partial |
| AMR Application Design | Draft |
| Edge Server Architecture | Partial |
| Edge Data Model | Draft |
| Task State Machine | Draft |
| HTTP Error Codes | Draft |
| AMR Pending Reports Format | Draft |
| Simulation Architecture | Partial |
| Interface Contract | Draft |
| V2 Traffic Control Chain | Planned（设计已冻结） |
| V2 Implementation Spec | Planned（设计已冻结） |

## 权威需求

- [软件需求规格说明书](requirements/srs.md)：V1 需求、范围、验收标准和 TBD。
- [接口契约](requirements/interface-contract.md)：Edge HTTP、MQTT 任务消息、ROS2 接口名称和 QoS/幂等规则。
- [HTTP 错误码](requirements/http-error-codes.md)：Edge HTTP API 的统一响应和错误码。
- [AMR 待补报文件格式](requirements/pending-task-reports-format.md)：`pending_task_reports.json` 结构、写入和清理规则。
- [项目待办与交付清单](TODO.md)：后续开发、测试和验收的工作清单。
- [构建与启动说明](development/build-run.md)：Edge、ROS2 workspace、AMR 启动命令。
- [导航地图/代价图排查记录](development/troubleshooting-navigation.md)：namespace、TF、lifecycle、initialpose 时序的分层排查案例。

## 架构文档

- [系统总体架构](architecture/system.md)
- [AMR 内部架构](architecture/amr.md)
- [AMR Application 设计](architecture/amr-application-design.md)
- [Edge Server 架构](architecture/edge_server.md)
- [Edge 数据模型与 SQLite Schema](architecture/edge-data-model.md)
- [任务状态机与 AMR 行为状态机](architecture/task-state-machine.md)
- [Simulation 架构](architecture/simulation.md)
- [架构决策记录](architecture/adr/README.md)
- [多版本开发路线图与版本边界](architecture/version-roadmap.md)：V1.5 / V2 / V3 / V4 范围、边界、功能细节与验收（内部开发任务书）
- [V2 交通路权控制链](architecture/roadmap/v2-traffic-control-chain.md)：`amr_behavior_manager`(TrafficGate) / `amr_agent`(TrafficBridge) / Edge 的进程边界、逐跳数据链路、取消导航调用链、五场景时序与实现者检查项（**模块归属与 topic 命名已于 2026-11-20 定案；几何与判据于 2026-11-22 按 V0.20 定案改写为 4 点通行控制区 + 中心点进出边沿 + 无 `plan` 订阅**）
  - 配套图（人类阅读版）：[进程边界与数据流](assets/v2-traffic-dataflow.svg) · [五场景时序](assets/v2-traffic-sequence.svg)
  - **[图的可读文字版](architecture/roadmap/v2-traffic-diagrams.md)**：两张 SVG 的 mermaid 纯文字转写，**供写代码的 agent / 纯文本工具读取**（`flowchart` + 4 张 `sequenceDiagram` + `stateDiagram-v2`）；语义真值仍是控制链文档 §4 逐跳表 / §6 五场景
- [V2 实现规格与功能流程](architecture/roadmap/v2-implementation-spec.md)：**给实现者直接照着写代码的开工依据** —— 已冻结命名与消息字段表、模块/类清单、`TrafficGate` 状态机、逐步伪代码、Edge 仲裁伪代码、全场景功能流程、配置项、错误与竞态处理、实现顺序与自检清单

## 已确认关键决策

- 需求权威文档为 `docs/requirements/srs.md`。
- README 采用“V1 当前范围 + 未来愿景”结构。
- V1 暂不实现 Cloud Platform，采用 Edge 侧任务注入。
- 仿真实例字段统一使用 `instance_id`。
- V2 路权逻辑并入 `amr_behavior_manager`（**不设独立模块、不新建 ROS2 包**）；`amr_power_manager` 为 V2 独立模块。
- V2 路权专用 ROS2 topic 为 `traffic_request` / `traffic_response`；MQTT 为 `amr/{robot_id}/traffic_rights/request` / `response`，动作由 payload `op` 承载。
- V2 需求已纳入 [SRS](requirements/srs.md) 第 4.10 / 4.11 节（V0.20）。
- Cloud → Edge 任务使用 `robot_ids` 指定参与 AMR；Edge → AMR 只下发总量和货物列表。
- Edge → AMR 任务 Topic 为 `amr/{robot_id}/task`，共享进度 Topic 为 `warehouse/{warehouse_id}/task/{task_id}/state`。
- 共享进度消息必须包含 `goods_status`，按货物拆分搬运进度。
- AMR 搬运前通过 `amr/{robot_id}/task/goods/request` 申请，等待响应成功；正在搬运货物的 AMR 列表仅保存在 Edge 内部。
- AMR 通过 `amr/{robot_id}/task/status` 上报状态、完成和放弃；全部 AMR 异常时 Edge 上报任务级 `INTERRUPTED`。
- AMR 任务状态消息使用 `goods` 数组，支持一趟搬运多类货物。
- Edge HTTP 任务注入无鉴权；AMR 心跳 5 秒，Edge 超时判定 5 分钟。
- Edge 异常时 AMR 完成当前已获批搬运后停止，本地待补报写入 `pending_task_reports.json`，Edge 恢复后合并。
- Edge 通过 `amr/{robot_id}/task/status/ack` 确认 AMR 消息；AMR 收到 ACK 后清理本地待补报记录。
- `amr_agent` 与 `amr_behavior_manager` 通过 `task_command` / `task_event` 通信，并新增 TaskCommand/TaskEvent 消息。
- `map -> zone -> station -> location` 是货物点位映射关系；zone 使用 `goods_ids`，任务不携带 target 字段。
- V1 先假设所有 zone 均为货物相关区域。

未确认事项继续以 TBD 记录，不在本索引中自行填充。

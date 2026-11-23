# AGENTS.md

本文件是 AMR 仓库的开发协作规则，所有后续文档修改和代码开发都应遵守。

## 1. 需求与版本边界

- 需求权威文档为 `docs/requirements/srs.md`。
- V1 只实现 SRS 中明确标记为 V1 且优先级为 Must 的需求。
- V2 及后续版本规划不得自动转化为 V1 开发任务。
- V1 不实现交通路权协调、电量管理、Cloud Platform、机械臂、视觉、完整 WMS/Web 平台。V2 的路权逻辑**并入 `amr_behavior_manager`（不设独立模块）**，电量管理由 `amr_power_manager` 承担。

## 2. 文档规则

- 文档统一使用 Markdown。
- 架构文档位于 `docs/architecture/`，需求文档位于 `docs/requirements/`。
- 新增或修改接口、Topic、消息字段、状态机、数据模型时，必须同步更新 SRS、接口契约或架构文档。
- 尚未确定的事项必须标记为 TBD，并写明需要确认的具体问题。
- 不得为了“让文档看起来完整”而自行填充未知信息。
- 文档应区分：
  - 已确定的架构决策；
  - 已实现的代码能力；
  - 仅规划但尚未实现的能力。
- 文档状态统一使用 `Implemented / Partial / Planned` 标注。

## 3. 命名与接口规则

- AMR 行为编排模块统一使用 `amr_behavior_manager`。
- 仿真实例字段统一使用 `instance_id`。
- 机器人业务身份使用 `robot_id`。
- V1 使用 Edge 侧 HTTP API 注入任务；后续 Cloud 通过 MQTT 发布任务，Edge 订阅任务 Topic。
- 任务 MQTT Topic 使用 `warehouse/{warehouse_id}/task` 和 `warehouse/{warehouse_id}/task/status`，QoS 1。
- Edge → AMR 任务 Topic 使用 `amr/{robot_id}/task`，任务级共享状态 Topic 使用 `warehouse/{warehouse_id}/task/{task_id}/state`。
- Edge → AMR 任务共享状态必须包含 `goods_status`，按货物拆分 `total_quantity`、`in_progress_quantity`、`completed_quantity`、`pending_quantity`。
- `robot_ids` 不能为空；V1 暂不支持 Edge 自动选择参与 AMR。
- AMR 每次搬运前必须通过 `amr/{robot_id}/task/goods/request` 向 Edge 申请，等待响应成功后才能开始。
- 正在搬运货物的 AMR 列表属于 Edge 内部状态，不下发给其他 AMR。
- AMR 通过 `amr/{robot_id}/task/status` 定时上报任务状态，并复用该 Topic 上报 `CARRY_COMPLETED` 和 `CARRY_ABORTED`。
- AMR 任务状态消息中的货物使用 `goods` 数组表示，支持同一趟搬运包含多类货物。
- 仅当参与任务的全部 AMR 异常时，Edge 向 Cloud 上报任务级 `INTERRUPTED`。
- Edge 任务注入 HTTP API 无鉴权，允许局域网内调用，响应统一为 `code + msg + data`。
- AMR 任务状态每 5 秒上报一次；Edge 5 分钟未收到更新即视为异常。
- 目的地由 `goods_id + task_type` 映射，不在任务消息中下发具体坐标。
- Edge 异常时，AMR 完成当前已获批搬运后停止下一行动；本地待补报结果持久化到 `pending_task_reports.json`。
- Edge 恢复后从 SQLite 恢复任务状态，按幂等和时间戳合并 AMR 待补报结果。
- Edge 通过 `amr/{robot_id}/task/status/ack` 确认 AMR 状态/完成/放弃消息；AMR 收到 ACK 后清理本地待补报记录。
- `amr_agent` 与 `amr_behavior_manager` 通过 ROS2 Topic `task_command` 和 `task_event` 通信。
- 新增 `TaskGoods.msg`、`TaskGoodsStatus.msg`、`TaskCommand.msg`、`TaskEvent.msg`。
- 货物申请由 `amr_behavior_manager` 决策，`amr_agent` 负责 MQTT 请求；导航失败不自动重试。
- `map -> zone -> station -> location` 是货物点位映射关系；`zones.yaml` 中 zone 使用 `goods_ids`。
- 任务消息不携带 target zone/station/location，由 AMR 按 `goods_id + task_type` 推导。
- V1 先假设所有 zone 均为货物相关区域；非货物区域后续再增加字段区分。**V2 的窄道通行控制区走独立 `traffic_gates.yaml`，不改动 V1 的 `map -> zone -> station -> location` 层级与 `zones.yaml` / `stations.yaml` 的 `type` 取值。**
- JSON 消息字段使用 `snake_case`；任务状态枚举与 SRS 保持一致。
- AMR 内部 ROS2 Topic/Service/Action 名称统一定义在 `amr_common`。
- Cloud/Edge/AMR 相关 MQTT Topic 统一定义在 `common/mqtt/mqtt_topic.h`。
- AMR 内部使用 ROS2；AMR 与 Edge Server 使用 HTTP/MQTT；Simulation Manager 与 Edge Server 使用 WebSocket。
- 模块之间通过明确接口交互，避免直接依赖其他模块内部实现。
- 重要架构决策记录在 `docs/architecture/adr/`。
- **V2 路权模块归属（2026-11-20 定案）**：不设独立模块，实现为 `amr_behavior_manager` 包内的 `TrafficGate`（类 + BT 节点）。全仓文档与代码**不得再使用已废弃的独立交通模块命名**，避免实现者误以为存在独立进程 / ROS2 包。
- **V2 路权专用 ROS2 topic（2026-11-20 定案）**：`traffic_request`（常量 `amr_common::topic::TRAFFIC_REQUEST`，`amr_behavior_manager` → `amr_agent`）与 `traffic_response`（常量 `amr_common::topic::TRAFFIC_RESPONSE`，`amr_agent` → `amr_behavior_manager`）；消息 `amr_interfaces/msg/TrafficRequest.msg` / `TrafficResponse.msg`。**不得复用 `task_command` / `task_event`**。
- **V2 路权 MQTT topic（2026-11-20 定案）**：上行 `amr/{robot_id}/traffic_rights/request`，下行单播 `amr/{robot_id}/traffic_rights/response`；Edge 订阅 `amr/+/traffic_rights/request`。动作语义由 payload `op` 承载（`REQUEST` / `CANCEL` / `RELEASE`），**不采用动作分层 topic**。
- **V2 电量 topic（D 方案）**：`power_mode` / `power_level`，消息 `PowerMode.msg` / `BatteryState.msg`。
- **V2 窄道几何与权限判据（2026-11-22 第十三轮定案，V0.20）**：窄道数据为独立 `traffic_gates.yaml`（`resource_id` + 4 点凸 `polygon` + `wait_poses{l1,l2}`，每侧 ≥1），**不扩展** `zones.yaml` / `stations.yaml` 的 `type`、不新增 `NARROW` / `TRAFFIC_WAIT` / `side` 字段；点序冻结 `P1→P2` 端边 L1 / `P2→P3` 长轴 / `P3→P4` 端边 L2。主判据为 **AMR 中心点进出区域的 `inside` 边沿**（action feedback `current_pose` + 边沿检测 + 滞回；禁用 1 Hz `robot_pose`），**不是**跨线事件、不是 footprint；进入侧由 `dot(enter_pose - centroid, long_axis)` 符号确定并**在本次穿越周期锁存**，等待点在**同侧组内**就近选取且位于入口端边外侧。**全程不订阅 `plan`**，不设 `onPlan()` / `onPlanChanged()` / `D_trigger`；拉长 `L_pull` 自带提前量；改道释放语义由「出区边沿 / TTL」承接，无独立改道检测机制。`REQUESTING` 收 `GRANTED` 零动作，仅 `WAITING` 获批才重发 D。`MapInfo.msg` 加 `traffic_gates_yaml` 字段（地图包第 4 文件）；工具侧新建 `traffic_gates_editor` 两阶段（4 点 zone + 每侧 ≥1 等待点），复用 `ClickedPointReceiver` / `ZoneMarkerPublisher`，**不复用** `zone_editor` 节点本体 / `YamlZoneManager` / `ZoneInfo` / `selectZoneType()`。权威口径见 `.workbuddy/artifacts/traffic-gate-8-decisions-frozen.md`。
- **V2 图的可读文字版（2026-11-22）**：`docs/assets/v2-traffic-dataflow.svg` / `v2-traffic-sequence.svg` 两张图**仅供人类阅读**，写代码的 agent 读 `docs/` 时读不到图内文字；其纯文字等价物是 `docs/architecture/roadmap/v2-traffic-diagrams.md`（mermaid `flowchart` / `sequenceDiagram` / `stateDiagram-v2`）。**语义真值仍是 `v2-traffic-control-chain.md` §4 逐跳表与 §6 五场景**；新增或修改跨模块链路、topic、状态转移时，须同步「SVG + mermaid + 控制链文档」三处，避免图与文字口径分叉。

## 4. 开发规则

- 不为了统一风格而大规模重构现有代码。
- 不删除现有功能。
- 不擅自修改构建文件、依赖配置或业务代码，除非任务明确允许。
- 新增功能前应先完成对应文档和接口决策。
- 提交说明应关联需求编号、验收项和必要文档。
- 每次完成功能或文档后，必须说明“为什么这么实现”，包含设计动机、边界和取舍，便于面试表达和后续维护。

## 5. 当前 TBD 处理

- V1 的 Edge 任务注入入口已确定为 HTTP API。
- 任务相关 MQTT Topic、消息结构、QoS 和幂等字段已记录在接口契约和 ADR-0001。
- `task_state` 状态机已冻结。
- Edge 恢复后不自动继续，需人工或上层重新创建任务。
- Cloud Platform 后续形态尚未确定。
- `amr_agent` CMake 中的 `amr_common` 依赖已补充。

在相关事项确认前，不得假设实现方式。

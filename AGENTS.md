# AGENTS.md

本文件是 AMR 仓库的开发协作规则，所有后续文档修改和代码开发都应遵守。

## 1. 需求与版本边界

- 需求权威文档为 `docs/requirements/srs.md`。
- V1 只实现 SRS 中明确标记为 V1 且优先级为 Must 的需求。
- V2 及后续版本规划不得自动转化为 V1 开发任务。
- `amr_traffic_manager`、`amr_power_manager`、Cloud Platform、机械臂、视觉、完整 WMS/Web 平台均不属于 V1 交付范围。

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
- JSON 消息字段使用 `snake_case`；任务状态枚举与 SRS 保持一致。
- AMR 内部 ROS2 Topic/Service/Action 名称统一定义在 `amr_common`。
- Cloud/Edge/AMR 相关 MQTT Topic 统一定义在 `common/mqtt/mqtt_topic.h`。
- AMR 内部使用 ROS2；AMR 与 Edge Server 使用 HTTP/MQTT；Simulation Manager 与 Edge Server 使用 WebSocket。
- 模块之间通过明确接口交互，避免直接依赖其他模块内部实现。
- 重要架构决策记录在 `docs/architecture/adr/`。

## 4. 开发规则

- 不为了统一风格而大规模重构现有代码。
- 不删除现有功能。
- 不擅自修改构建文件、依赖配置或业务代码，除非任务明确允许。
- 新增功能前应先完成对应文档和接口决策。
- 提交说明应关联需求编号、验收项和必要文档。

## 5. 当前 TBD 处理

- V1 的 Edge 任务注入入口已确定为 HTTP API。
- 任务相关 MQTT Topic、消息结构、QoS 和幂等字段已记录在接口契约和 ADR-0001。
- `task_state` 的完整状态机尚未确定。
- Edge 恢复后 AMR 是否自动继续后续任务尚未确定。
- Cloud Platform 后续形态尚未确定。
- `amr_agent` CMake 中的 `amr_common` 依赖已补充。

在相关事项确认前，不得假设实现方式。

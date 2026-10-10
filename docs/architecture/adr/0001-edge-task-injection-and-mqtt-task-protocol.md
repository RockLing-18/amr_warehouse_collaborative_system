# ADR-0001: V1 Edge 任务注入与 MQTT 任务消息协议

## 状态

Accepted

## 日期

2026-10-10

## 背景

V1 当前没有 Cloud Platform，但 SRS 需要验证运输任务闭环。任务创建入口、任务消息结构和状态上报方式需要提前冻结，避免 Edge、AMR 和未来 Cloud 各自定义不同协议。

## 决策

1. V1 使用 Edge 侧 HTTP API 作为任务注入入口。
2. 后续 Cloud 实现后保留该 HTTP API；同时 Cloud 通过 MQTT 发布任务，Edge 订阅任务 Topic。
3. Cloud → Edge 任务 Topic 使用 `warehouse/{warehouse_id}/task`，QoS 1。
4. Edge → Cloud 任务状态 Topic 使用 `warehouse/{warehouse_id}/task/status`，QoS 1。
5. Edge → AMR 任务下发 Topic 使用 `amr/{robot_id}/task`，QoS 1。
6. Edge → AMR 任务共享状态 Topic 使用 `warehouse/{warehouse_id}/task/{task_id}/state`，QoS 1。
7. 任务消息 JSON 使用 `snake_case`。
8. Cloud → Edge 任务消息使用 `robot_ids` 表示参与搬运的 AMR 列表。
9. `robot_ids` 不能为空；V1 暂不支持 Edge 自动选择参与 AMR。
10. Edge → AMR 任务消息只下发任务总量和货物列表，不下发按 AMR 平均分配的搬运量。
11. AMR 每次搬运前通过 `amr/{robot_id}/task/goods/request` 向 Edge 申请货物和数量，等待 `amr/{robot_id}/task/goods/response` 成功后才能开始。
12. 正在搬运货物的 AMR 列表属于 Edge 内部状态，不进入任务共享状态。
13. AMR 通过 `amr/{robot_id}/task/status` 定时上报任务状态，并复用该 Topic 上报 `CARRY_COMPLETED` 和 `CARRY_ABORTED`。
14. 该消息中的货物使用 `goods` 数组表示，支持同一趟搬运包含多类货物。
15. `CARRY_COMPLETED` 对 `goods` 数组逐项将 `in_progress_quantity` 转为 `completed_quantity`；`CARRY_ABORTED` 对 `goods` 数组逐项将 `in_progress_quantity` 回退为 `pending_quantity`。
16. 如果 `robot_ids` 中仍有可用 AMR，任务继续；只有全部 AMR 异常时，Edge 才向 Cloud 上报任务级 `INTERRUPTED`。
17. 任务类型使用 `UNLOAD` / `LOAD` 枚举。
18. 任务状态使用 `PENDING`、`ACCEPTED`、`RUNNING`、`SUCCEEDED`、`FAILED`、`CANCELLED`、`INTERRUPTED`。
19. Edge → AMR 任务共享状态必须包含 `goods_status`，按货物拆分 `total_quantity`、`in_progress_quantity`、`completed_quantity`、`pending_quantity`。
20. Edge 使用 `task_id` + `message_id` 做幂等和去重，QoS 1 仅保证传输可靠性。
21. AMR 内部 ROS2 Topic/Service/Action 名称统一定义在 `amr_common`。
22. Cloud/Edge/AMR 相关 MQTT Topic 统一定义在 `common/mqtt/mqtt_topic.h`。
23. Edge 任务注入 HTTP API 无鉴权，允许局域网内调用，统一使用 `code + msg + data`。
24. AMR 任务状态每 5 秒上报一次；Edge 5 分钟未收到更新即视为异常。
25. AMR 申请货物使用 `goods` 数组，必须参考 Edge 最新 `goods_status.pending_quantity`。
26. 目的地由 `goods_id + task_type` 映射，完成上报暂不包含 `from/to/station_id`。
27. V1 第一验收闭环包含任务创建、货物申请、完成上报和导航验证；抓取动作先以数据变化代替。
28. Edge 异常时，AMR 完成当前已获批搬运后停止下一行动，不进行本地平均分配。
29. AMR 本地待补报结果持久化到 `pending_task_reports.json`，采用原子写入。
30. Edge 恢复后从 SQLite 恢复任务状态，按幂等和时间戳合并 AMR 待补报结果。

## 理由

- HTTP 任务注入可以让 V1 在没有 Cloud Platform 的情况下快速形成闭环。
- MQTT 任务 Topic 为后续 Cloud 与 Edge 解耦提供稳定接口。
- QoS 1 适合任务下发和状态上报这类至少一次的传输需求。
- 幂等字段和状态机约束可以避免重复消息导致重复任务或状态回退。

## 影响

- Edge Server 后续需要增加任务 HTTP API、任务持久化、任务 MQTT 订阅和状态发布。
- Edge Server 需要维护任务级共享进度，并向参与 AMR 发布 `total_quantity`、`in_progress_quantity` 等状态。
- `common/mqtt/mqtt_topic.h` 需要维护任务 Topic 常量。
- `amr_common` 需要维护 ROS2 接口名称常量。
- 接口契约文档需要随该 ADR 同步更新。

## 关联文档

- `docs/requirements/interface-contract.md`
- `docs/requirements/srs.md`

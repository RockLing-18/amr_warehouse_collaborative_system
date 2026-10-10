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
| Edge Server Architecture | Partial |
| Simulation Architecture | Partial |
| Interface Contract | Draft |

## 权威需求

- [软件需求规格说明书](requirements/srs.md)：V1 需求、范围、验收标准和 TBD。
- [接口契约](requirements/interface-contract.md)：Edge HTTP、MQTT 任务消息、ROS2 接口名称和 QoS/幂等规则。

## 架构文档

- [系统总体架构](architecture/system.md)
- [AMR 内部架构](architecture/amr.md)
- [Edge Server 架构](architecture/edge_server.md)
- [Simulation 架构](architecture/simulation.md)
- [架构决策记录](architecture/adr/README.md)

## 已确认关键决策

- 需求权威文档为 `docs/requirements/srs.md`。
- README 采用“V1 当前范围 + 未来愿景”结构。
- V1 暂不实现 Cloud Platform，采用 Edge 侧任务注入。
- 仿真实例字段统一使用 `instance_id`。
- `amr_traffic_manager`、`amr_power_manager` 属于后续版本。
- Cloud → Edge 任务使用 `robot_ids` 指定参与 AMR；Edge → AMR 只下发总量和货物列表。
- Edge → AMR 任务 Topic 为 `amr/{robot_id}/task`，共享进度 Topic 为 `warehouse/{warehouse_id}/task/{task_id}/state`。
- 共享进度消息必须包含 `goods_status`，按货物拆分搬运进度。
- AMR 搬运前通过 `amr/{robot_id}/task/goods/request` 申请，等待响应成功；正在搬运货物的 AMR 列表仅保存在 Edge 内部。
- AMR 通过 `amr/{robot_id}/task/status` 上报状态、完成和放弃；全部 AMR 异常时 Edge 上报任务级 `INTERRUPTED`。
- AMR 任务状态消息使用 `goods` 数组，支持一趟搬运多类货物。
- Edge HTTP 任务注入无鉴权；AMR 心跳 5 秒，Edge 超时判定 5 分钟。
- Edge 异常时 AMR 完成当前已获批搬运后停止，本地待补报写入 `pending_task_reports.json`，Edge 恢复后合并。

未确认事项继续以 TBD 记录，不在本索引中自行填充。

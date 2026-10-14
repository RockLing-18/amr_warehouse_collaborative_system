# AMR 项目待办与交付清单

## 1. 目的

本文件是 AMR 项目从“文档冻结”到“V1 可用”的工作清单。所有后续开发、测试和验收都围绕本文件推进。

它回答三个问题：

1. 下一步做什么。
2. 每项任务的交付物是什么。
3. 如何验证任务已经完成。

## 2. 当前基线

- 需求权威文档：[requirements/srs.md](requirements/srs.md)
- 接口契约：[requirements/interface-contract.md](requirements/interface-contract.md)
- Edge 数据模型：[architecture/edge-data-model.md](architecture/edge-data-model.md)
- 状态机设计：[architecture/task-state-machine.md](architecture/task-state-machine.md)
- 架构文档：[architecture/](architecture/)
- 架构决策记录：[architecture/adr/](architecture/adr/)
- 开发规则：[../AGENTS.md](../AGENTS.md)

当前 SRS 版本：V0.12。

当前执行：修复 Station Editor 输入竞争问题，点选 location 后由统一终端线程处理 type/yaw，避免 stdin 并发读取；编译通过。

## 3. 状态图例

| 状态 | 含义 |
| --- | --- |
| Not Started | 尚未开始 |
| In Progress | 正在执行 |
| Blocked | 依赖未确认，无法继续 |
| Done | 已交付并通过验证 |

## 4. 总里程碑

| 里程碑 | 目标 | 主要输出 |
| --- | --- | --- |
| M0 | 文档基线冻结 | SRS、接口契约、ADR、TODO |
| M1 | 数据模型与状态机冻结 | Edge 数据模型、任务状态机、AMR 状态机 |
| M2 | 接口与消息体完整定义 | HTTP API、MQTT 消息、ROS2 接口、错误码 |
| M3 | Edge Server V1 可用 | 任务注入、审批、状态聚合、持久化、恢复 |
| M4 | AMR Application V1 可用 | 任务接收、申请、完成上报、导航、心跳、补报 |
| M5 | Simulation V1 可用 | 多 AMR 生命周期、WebSocket 同步、地图/世界 |
| M6 | 集成验证通过 | 端到端任务闭环、异常场景、验收记录 |
| M7 | V1 发布基线 | 文档、构建说明、测试证据、README |

## 5. 详细待办

### 5.1 SRS 完善

| ID | 任务 | 交付物 | 验证方式 | 状态 |
| --- | --- | --- | --- | --- |
| SRS-01 | 补充需求实现状态矩阵 | SRS 中增加 `Implemented / Partial / Planned` 状态 | 每个 FR 都能对应到代码或标记为 Planned | Done |
| SRS-02 | 补充任务状态机说明 | 任务级状态转换表或图 | 所有状态和转换可被验收 | Not Started |
| SRS-03 | 补充 AMR 行为状态机说明 | AMR `task_state` 转换表或图 | 覆盖抓取、搬运、放置、中断 | Not Started |
| SRS-04 | 明确 Edge HTTP 错误码 | 接口契约/SRS 错误码表 | 每个错误场景有唯一错误码 | Not Started |
| SRS-05 | 明确验收证据要求 | 每项 AC 增加步骤、预期结果、证据 | 验收人员可以按文档执行 | Not Started |
| SRS-06 | 清理已解决 TBD | 更新 TBD 表 | 文档中无过期 TBD | In Progress |
| SRS-07 | 补充 Edge 故障恢复验收场景 | 验收标准 | 能验证 Edge 重启后任务进度合并 | Not Started |

### 5.2 接口与消息体

| ID | 任务 | 交付物 | 验证方式 | 状态 |
| --- | --- | --- | --- | --- |
| API-01 | 冻结 Edge 任务注入 HTTP API | 请求/响应/错误码/Schema | 用 curl 或脚本可验证 | Done |
| API-02 | 冻结任务 MQTT 消息 | `warehouse/{}/task` 消息 Schema | 消息样例和字段约束完整 | Not Started |
| API-03 | 冻结任务状态消息 | `warehouse/{}/task/status` 消息 Schema | 状态枚举、原因枚举完整 | Not Started |
| API-04 | 冻结 AMR 任务消息 | `amr/{}/task` 消息 Schema | 与实现代码一致 | Not Started |
| API-05 | 冻结货物申请/响应消息 | `goods/request`、`goods/response` Schema | 支持多货物申请 | Not Started |
| API-06 | 冻结 AMR 状态/完成/放弃消息 | `amr/{}/task/status` Schema | `goods` 数组、事件类型、原因枚举完整 | Not Started |
| API-07 | 冻结共享任务状态消息 | `warehouse/{}/task/{}/state` Schema | `goods_status` 与不变式完整 | Not Started |
| API-08 | 定义 MQTT QoS、retained、幂等规则 | 接口契约 | 消息去重、旧消息回退规则清晰 | Not Started |

### 5.3 数据模型与持久化

| ID | 任务 | 交付物 | 验证方式 | 状态 |
| --- | --- | --- | --- | --- |
| DB-01 | 设计 Edge SQLite Schema | 数据模型文档 | 表、索引、唯一约束完整 | Done |
| DB-02 | 设计任务货物进度表 | `task_goods_status` 设计 | 能表达 total/in_progress/completed/pending | Done |
| DB-03 | 设计待补报/事件表 | `task_event`、`pending_report` 设计 | 能支持幂等合并 | Done |
| DB-04 | 设计 AMR 本地待补报文件格式 | `pending_task_reports.json` 规范 | 能原子写入并恢复 | Done |
| DB-05 | 确认 Edge 恢复时数据合并算法 | 文档/伪代码 | 重复消息不重复累计 | Done |

### 5.4 状态机与业务规则

| ID | 任务 | 交付物 | 验证方式 | 状态 |
| --- | --- | --- | --- | --- |
| SM-01 | 定义任务级状态机 | 状态转换表 | 覆盖正常、失败、取消、中断 | Done |
| SM-02 | 定义 AMR 行为状态机 | AMR `task_state` 转换表 | 覆盖抓取、搬运、放置、中断 | Done |
| SM-03 | 定义货物申请/审批规则 | 业务规则文档 | 超量申请被拒绝 | Not Started |
| SM-04 | 定义 AMR 异常判定规则 | 心跳、超时、自报中断规则 | 5s 心跳、5min 超时 | Not Started |
| SM-05 | 定义 Edge 故障恢复规则 | 业务规则文档 | 安全停止、补报、合并 | Not Started |

### 5.5 Edge Server 编码

| ID | 任务 | 交付物 | 验证方式 | 状态 |
| --- | --- | --- | --- | --- |
| EDGE-01 | 实现任务 HTTP 注入 API | Edge 代码 | `POST /api/tasks` 创建任务 | Done |
| EDGE-02 | 实现任务查询/取消 API | Edge 代码 | 查询和取消返回正确状态 | Done |
| EDGE-03 | 实现任务持久化 | SQLite 表与读写 | 重启后任务可恢复 | Done |
| EDGE-04 | 实现货物申请审批 | Edge Task Service | 申请不超过 pending 时批准 | Not Started |
| EDGE-05 | 实现任务共享状态发布 | MQTT 发布 | AMR 能收到 `goods_status` | Not Started |
| EDGE-06 | 实现 AMR 完成/放弃处理 | Edge 状态聚合 | 数量从 in_progress 正确转移 | Not Started |
| EDGE-07 | 实现 AMR 心跳超时判定 | Edge 逻辑 | 5 分钟无更新标记异常 | Not Started |
| EDGE-08 | 实现 Edge 重启恢复 | SQLite 加载 + 补报合并 | 重启后进度不丢、不重复 | Not Started |
| EDGE-09 | 实现全 AMR 异常任务中断 | Edge 状态机 | 全部异常时上报 `INTERRUPTED` | Not Started |

### 5.6 AMR Application 编码

| ID | 任务 | 交付物 | 验证方式 | 状态 |
| --- | --- | --- | --- | --- |
| AMR-01 | 实现任务接收 | `amr_agent` | 收到 Edge 任务后进入行为管理 | In Progress |
| AMR-02 | 实现定时任务状态上报 | `amr_agent` | 每 5 秒上报 `STATUS` | In Progress |
| AMR-03 | 实现货物申请 | `amr_agent`/`amr_behavior_manager` | 申请成功后进入搬运流程 | In Progress |
| AMR-04 | 实现完成/放弃上报 | `amr_agent` | 到达目的地后上报 `CARRY_COMPLETED` | In Progress |
| AMR-05 | 实现本地待补报文件 | AMR 本地持久化 | 断网时结果不丢失 | In Progress |
| AMR-06 | 实现 Edge 故障检测 | AMR 状态监控 | Edge 异常时停止下一行动 | Not Started |
| AMR-07 | 实现导航调用 | `amr_behavior_manager` + `amr_navigation` | 能前往指定点位 | In Progress |
| AMR-08 | 实现导航取消/失败处理 | `amr_behavior_manager` | 取消后正确更新任务状态 | Not Started |
| AMR-09 | 实现多 AMR 命名空间和身份隔离 | 启动配置 | 两个 AMR 可同时运行 | Not Started |

### 5.7 Simulation 编码

| ID | 任务 | 交付物 | 验证方式 | 状态 |
| --- | --- | --- | --- | --- |
| SIM-01 | 实现多 AMR 生命周期管理 | `simulation_manager` | 创建、删除、替换 AMR | Partial |
| SIM-02 | 实现 WebSocket `robot_list` 同步 | `simulation_manager`/Edge | Edge 能看到仿真 AMR 列表 | Partial |
| SIM-03 | 实现控制器状态检查 | `controller_checker` | AMR 启动后控制器可用 | Partial |
| SIM-04 | 定义 `robot_id`/`instance_id` 分配规则 | 文档/配置 | 多 AMR 身份唯一 | Not Started |
| SIM-05 | 固化 Gazebo 世界和地图 | 配置/脚本 | 仿真可重复启动 | Not Started |

### 5.8 测试与验收

| ID | 任务 | 交付物 | 验证方式 | 状态 |
| --- | --- | --- | --- | --- |
| TEST-01 | 编写单元测试 | 测试代码 | 关键模块测试通过 | Not Started |
| TEST-02 | 编写接口测试 | HTTP/MQTT 测试脚本 | 接口字段和错误码符合契约 | Not Started |
| TEST-03 | 编写端到端正常流程测试 | 测试文档/脚本 | 任务创建→申请→完成→状态更新 | Not Started |
| TEST-04 | 编写取消流程测试 | 测试脚本 | 取消后状态为 `CANCELLED` | Not Started |
| TEST-05 | 编写断网/恢复测试 | 测试脚本 | 结果补报、进度合并正确 | Not Started |
| TEST-06 | 编写多 AMR 异常测试 | 测试脚本 | 部分异常继续，全部异常中断 | Not Started |
| TEST-07 | 编写构建和启动文档 | `docs/development/build-run.md` | 按文档可复现构建 | Not Started |
| TEST-08 | 整理 V1 验收证据 | 日志、截图、命令输出 | 所有 Must AC 有证据 | Not Started |

### 5.9 文档与发布

| ID | 任务 | 交付物 | 验证方式 | 状态 |
| --- | --- | --- | --- | --- |
| DOC-01 | 更新 README | 根 README | 与 V1 范围一致 | Done |
| DOC-02 | 补充数据模型文档 | 数据模型文档 | 表结构完整 | Not Started |
| DOC-03 | 补充状态机文档 | 状态机文档 | 转换完整 | Not Started |
| DOC-04 | 补充 Edge 模块设计 | 架构/设计文档 | 服务、通信、存储边界清晰 | Not Started |
| DOC-05 | 补充 AMR 模块设计 | 架构/设计文档 | Agent/Behavior/Navigation 边界清晰 | Done |
| DOC-06 | 更新 TODO 状态 | 本文件 | 每完成一项更新状态 | In Progress |

## 6. 当前仍未解决的关键问题

以下问题继续阻碍对应任务，需要逐步确认：

1. Edge 恢复后 AMR 是否自动继续后续任务。
2. Edge HTTP API 具体错误码枚举。
3. MQTT 是否需要独立 ACK Topic。
4. `task_state` 完整状态转换。
5. 货物点位映射表的维护方式。
6. 地图包内部布局和下载地址生成规则。
7. AMR 待补报文件的清理时机和保留时长。
8. MQTT 最小认证策略。
9. 断网期间取消任务的处理规则。
10. Edge 与 AMR 状态冲突的最终裁决规则。

## 7. 下一步建议

当前已有 `edge-data-model.md` 和 `task-state-machine.md` 初稿，下一步应优先：

1. 复核并冻结数据模型与状态机中的“提议”项。
2. 完成 SRS 实现状态矩阵。
3. 补齐 AMR 本地待补报文件格式。
4. 冻结 HTTP API 错误码和消息 Schema 剩余细节。

## 8. V1 完成定义

当以下条件全部满足时，V1 可视为完成：

- [ ] SRS Must 需求均有对应实现和验收证据。
- [ ] Edge HTTP 任务注入可用。
- [ ] 货物申请、批准、完成上报、放弃/中断流程可用。
- [ ] Edge 能正确维护并发布 `goods_status`。
- [ ] AMR 能完成导航、心跳、任务状态上报和断网补报。
- [ ] 至少两个 AMR 在仿真中同时运行且身份隔离。
- [ ] Edge 重启后任务进度可恢复，AMR 补报结果可合并。
- [ ] 部分 AMR 异常时任务继续，全部 AMR 异常时任务中断。
- [ ] 构建、启动、验收文档完整。
- [ ] 关键接口、消息体和错误码有文档约束。

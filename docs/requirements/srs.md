# 软件需求规格说明书（SRS）

> **⚠️ 2026-11-22 V0.20 定案变更**：V2 窄道路权以项目所有者 8 项拍板为唯一权威口径，见 [定案记录](../../.workbuddy/artifacts/traffic-gate-8-decisions-frozen.md)（`.workbuddy/artifacts/traffic-gate-8-decisions-frozen.md`）。旧 `NARROW` zone / `TRAFFIC_WAIT` / 有向触发段 / 外移释放线 / `crossed()` 主判据 / `plan` 只读预判 / `D_trigger` 口径**已废弃**，不得据此实现；本文 1.6、3.4.4、4.10、6.7、7、8、9、10 章已同步为「独立 `traffic_gates.yaml` + 4 点通行控制区 + 中心点进出边沿 + 无 `plan` 订阅」。

**项目名称：** AMR 云边协同多机器人协调系统
**英文名称：** Warehouse AMR Cloud-Edge Collaborative System
**文档版本：** V0.20
**当前开发基线：** V1（已交付）→ V2（设计已冻结，待实现）；V0.20 基线日期 2026-11-22
**文档状态：** Draft（V1 已按代码基线校对；**V2 需求已于 2026-11-20 正式纳入**；**V2 窄道路权 8 项定案已于 2026-11-22 同步为 V0.20**）
**最后更新：** 2026-11-22
**运行环境：** Ubuntu 22.04、ROS2 Humble、Gazebo 11

---

## 1. 项目概述

### 1.1 项目背景

本项目旨在构建一个基于 ROS2 的智能仓储 AMR（Autonomous Mobile Robot）云边协同多机器人运输系统。

系统由 Cloud Platform、Edge Server、AMR Application 和 Simulation Environment 四个主要部分组成。

Cloud Platform 负责上层业务管理和任务创建；Edge Server 负责仓库级服务、AMR 接入、任务下发及相关数据管理；AMR Application 负责任务接收、行为编排、导航执行及状态反馈；Simulation Environment 负责机器人仿真、生命周期管理以及仿真状态同步。

项目采用模块化架构，将机器人内部的运动执行能力与外部业务通信隔离，为后续扩展多机器人交通管理、电量管理和高级任务调度提供基础。

### 1.2 项目目标

本项目的主要目标如下：

1. 建立 Cloud、Edge、AMR、Simulation 之间职责清晰的系统架构。
2. 实现 AMR 启动初始化、Edge 注册、地图同步和通信建立。
3. 实现由 Edge 侧任务注入创建任务、Edge 下发任务、AMR 执行任务并反馈结果的 V1 业务闭环；Cloud 创建任务作为后续版本目标。
4. 使用 ROS2 和 Nav2 实现机器人内部模块协作及自主导航。
5. 使用 HTTP、MQTT 和 WebSocket 实现不同子系统之间的通信。
6. 使用 Gazebo 构建可重复运行的仓储 AMR 仿真环境。
7. 支持 V1 的任务取消，以及通信中断后的当前任务续行和结果补报。
8. 通过需求文档、架构文档、模块设计文档和验证流程建立规范的工程开发方式。

### 1.3 项目定位

本项目定位为具有完整业务闭环和模块化架构的工程实践及求职作品。

V1 优先保证核心功能能够运行、关键通信链路能够验证、主要模块职责明确，而不是一次性实现完整商业仓储系统的所有功能。

### 1.4 术语与约定

| 术语 | 说明 |
| --- | --- |
| AMR | Autonomous Mobile Robot，自主移动机器人。 |
| Cloud Platform | 负责业务管理、运输任务创建和结果查询的上层平台。 |
| Edge Server | 仓库侧独立 C++ 服务，负责 AMR 接入、地图管理、任务下发及状态汇聚。 |
| Simulation Manager | 负责 Gazebo 仿真 AMR 进程、模型及生命周期管理的组件。 |
| `robot_id` | 机器人业务身份，用于 Edge 和 AMR 之间关联机器人；后续 Cloud 版本复用。 |
| `instance_id` | 一次仿真运行中的实例标识，用于区分进程或 Gazebo 模型实例，不应与 `robot_id` 混淆。 |
| `warehouse_id` | 仓库标识，用于关联 Edge Server、地图包和机器人所属仓库。 |
| `task_id` | 运输任务唯一标识，跨任务注入调用方、Edge、AMR 关联同一任务。 |
| QoS | MQTT 或 ROS2 的服务质量策略。 |
| Bootstrap | AMR 启动时从 Edge Server 获取 MQTT 等接入配置的过程。 |

本文档中的“任务管理模块 / 行为编排模块”在 V1 代码包中对应 `amr_behavior_manager`。为避免文档与实现不一致，后续需求统一使用 `amr_behavior_manager` 作为该模块名称。

需求优先级采用 Must / Should / Could 进行区分。Must 为 V1 必须交付的核心能力，Should 为应尽量满足但可说明例外，Could 为可选能力。

### 1.5 V1 已确认决策

| 决策编号 | 决策内容 | 状态 |
| --- | --- | --- |
| D-001 | 需求权威文档为 `docs/requirements/srs.md`。 | Confirmed |
| D-002 | README 采用“V1 当前范围 + 未来愿景”的结构。 | Confirmed |
| D-003 | V1 暂不实现 Cloud Platform，采用 Edge 侧任务注入完成运输任务闭环。 | Confirmed |
| D-004 | 仿真实例字段统一使用 `instance_id`。 | Confirmed |
| D-005 | V1 不实现交通路权协调与电量管理；V2 路权逻辑并入 `amr_behavior_manager`（不设独立模块），电量管理由 `amr_power_manager` 承担。 | Confirmed |
| D-006 | V1 Edge 任务注入使用 HTTP API；后续 Cloud 保留该接口并通过 MQTT 发布任务。 | Confirmed |
| D-007 | 任务 MQTT Topic 使用 `warehouse/{warehouse_id}/task` 和 `warehouse/{warehouse_id}/task/status`，QoS 1。 | Confirmed |
| D-008 | 任务消息 JSON 使用 `snake_case`；任务状态枚举与 SRS 一致。 | Confirmed |
| D-009 | AMR 内部 ROS2 Topic/Service/Action 名称统一定义在 `amr_common`。 | Confirmed |
| D-010 | Cloud/Edge/AMR 相关 MQTT Topic 统一定义在 `common/mqtt/mqtt_topic.h`。 | Confirmed |
| D-011 | Cloud → Edge 任务消息使用 `robot_ids` 表示参与搬运的 AMR 列表。 | Confirmed |
| D-012 | Edge → AMR 任务下发 Topic 使用 `amr/{robot_id}/task`，任务消息只下发总量和货物列表，不做按 AMR 平均分配。 | Confirmed |
| D-013 | Edge → AMR 任务共享状态 Topic 使用 `warehouse/{warehouse_id}/task/{task_id}/state`。 | Confirmed |
| D-014 | Edge → AMR 任务共享状态必须包含 `goods_status`，按货物拆分搬运进度。 | Confirmed |
| D-015 | `robot_ids` 不能为空；V1 暂不支持 Edge 自动选择参与 AMR。 | Confirmed |
| D-016 | AMR 每次搬运前通过 `amr/{robot_id}/task/goods/request` 申请货物和数量，等待响应成功后才能开始。 | Confirmed |
| D-017 | 正在搬运货物的 AMR 列表属于 Edge 内部状态，不下发给 AMR。 | Confirmed |
| D-018 | AMR 通过 `amr/{robot_id}/task/status` 定时上报任务状态，并复用该 Topic 上报 `CARRY_COMPLETED` 和 `CARRY_ABORTED`。 | Confirmed |
| D-019 | AMR 任务状态消息中的货物使用 `goods` 数组表示，支持同一趟搬运包含多类货物。 | Confirmed |
| D-020 | 仅当参与任务的全部 AMR 异常时，Edge 向 Cloud 上报任务级 `INTERRUPTED`。 | Confirmed |
| D-021 | `CARRY_COMPLETED` 对 `goods` 数组逐项将 `in_progress_quantity` 转为 `completed_quantity`；`CARRY_ABORTED` 对 `goods` 数组逐项将 `in_progress_quantity` 回退为 `pending_quantity`。 | Confirmed |
| D-022 | Edge 任务注入 HTTP API 无鉴权，允许局域网内调用，统一使用 `code + msg + data` 响应结构。 | Confirmed |
| D-023 | AMR 任务状态每 5 秒上报一次；Edge 5 分钟未收到更新即视为异常。 | Confirmed |
| D-024 | AMR 申请货物使用 `goods` 数组，并参考 Edge 最新 `goods_status` 的 `pending_quantity`，不得超量申请。 | Confirmed |
| D-025 | 目的地不直接下发，由 `goods_id + task_type` 映射到装货区、卸载区或存储区；完成上报暂不包含 `from/to/station_id`。 | Confirmed |
| D-026 | V1 第一验收闭环包括任务创建、货物申请、完成上报和导航验证；抓取动作暂以数据变化代替。 | Confirmed |
| D-027 | Edge 异常时，AMR 完成当前已获批搬运后停止下一行动，不进行本地平均分配。 | Confirmed |
| D-028 | AMR 本地待补报结果持久化到 `pending_task_reports.json`，采用原子写入。 | Confirmed |
| D-029 | Edge 恢复后从 SQLite 恢复任务状态，按幂等和时间戳合并 AMR 待补报结果。 | Confirmed |
| D-030 | `ACCEPTED` 表示 Edge 已成功下发任务且至少一个参与 AMR 上报接受。 | Confirmed |
| D-031 | `RUNNING` 表示第一个货物申请被 Edge 批准。 | Confirmed |
| D-032 | V1 不主动使用 `FAILED`，保留给后续版本。 | Confirmed |
| D-033 | 任务级 `INTERRUPTED` 不自动继续，需人工或上层重新创建任务。 | Confirmed |
| D-034 | AMR 从 `INTERRUPTED` 恢复到 `IDLE`。 | Confirmed |
| D-035 | AMR 行为状态转换按 `IDLE -> ACCEPTED -> GRASPING -> TRANSPORTING -> PLACING -> IDLE/GRASPING`。 | Confirmed |
| D-036 | `PENDING`、`ACCEPTED`、`RUNNING` 均可进入 `CANCELLED`。 | Confirmed |
| D-037 | `tbl_task.robot_ids` 在 V1 使用 JSON 数组存储。 | Confirmed |
| D-038 | `tbl_task.goal` 在 V1 保留字段但允许为空。 | Confirmed |
| D-039 | V1 保留 `tbl_task_event` 和 `tbl_amr_task_status_event` 两张事件表。 | Confirmed |
| D-040 | 增加 `amr/{robot_id}/task/status/ack`，Edge 确认 AMR 消息，AMR 据此清理本地待补报记录。 | Confirmed |
| D-041 | `amr_agent` 与 `amr_behavior_manager` 使用 ROS2 Topic 通信，通过 `task_command` 和 `task_event` 解耦。 | Confirmed |
| D-042 | 新增 `TaskGoods.msg`、`TaskGoodsStatus.msg`、`TaskCommand.msg`、`TaskEvent.msg`。 | Confirmed |
| D-043 | 货物申请由 `amr_behavior_manager` 决策，`amr_agent` 负责 MQTT 请求。 | Confirmed |
| D-044 | 货物点位映射文件由 `amr_behavior_manager` 加载。 | Confirmed |
| D-045 | V1 的 `GRASPING` 和 `PLACING` 使用固定延时或状态直接推进。 | Confirmed |
| D-046 | `amr_behavior_manager` 发布内部 `task_status`，`amr_agent` 翻译为 MQTT `amr/{robot_id}/task/status`。 | Confirmed |
| D-047 | 导航失败时 V1 不自动重试，直接上报 `CARRY_ABORTED`，原因 `NAVIGATION_FAILED`。 | Confirmed |
| D-048 | `map -> zone -> station -> location` 为货物点位映射关系。 | Confirmed |
| D-049 | `zones.yaml` 中 zone 使用 `goods_ids` 列表记录可处理的货物。 | Confirmed |
| D-050 | 任务消息不携带 target zone/station/location，由 AMR 按 `goods_id + task_type` 推导。 | Confirmed |
| D-051 | V1 通过 `zone.type` 区分货物区域与充电区域（`IDLE`），其余 zone 默认货物相关；非货物区域后续再增加字段区分。 | Confirmed |
| D-052 | `location.type` 使用 `WORK` / `WAIT` / `IDLE`，`zone.type` 使用 `UNLOAD` / `LOAD` / `STORAGE` / `IDLE`，工具端以选择方式录入。 | Confirmed |
| D-053 | Edge 从当前生效地图包解析 `zones.yaml` / `stations.yaml`，将 `type=IDLE` 的 location 作为充电点，按顺序分配且检测占用。 | Confirmed |
| D-054 | Edge 在注册响应中返回 `initial_pose {x, y, yaw}`，AMR 据此发布初始位姿。 | Confirmed |
| D-055 | AMR 内部模块共享状态枚举（`ERROR < INITIALIZING < WORKING < IDLE`），`amr_agent` 取 `navigation_status` 与 `behavior_status` 的最小值作为 AMR 对外状态。 | Confirmed |
| D-056 | AMR TF 帧使用 `robot_id + "_"` 前缀（`map` 帧除外），`navigation_server` 按 `robot_id` 生成带前缀的 nav2 参数文件。 | Confirmed |

### 1.6 V2 已确认决策（2026-11-20 第七轮纳入）

> 本节由项目所有者授权写入，是 V2 需求的权威口径。实现任务书与取舍依据见 `docs/architecture/version-roadmap.md` 与 `docs/architecture/roadmap/v2-resource-arbitration-and-bt.md`；实现级细节见 `docs/architecture/roadmap/v2-implementation-spec.md`。

| 决策编号 | 决策内容 | 状态 |
| --- | --- | --- |
| D-057 | V2 路权逻辑**不设独立模块 / ROS2 包**，实现为 `amr_behavior_manager` 包内 `TrafficGate`（类 + BT 节点）；全仓禁止再以已废弃的独立模块名指代该逻辑。 | Confirmed |
| D-058 | V2 路权专用 ROS2 topic 固定为 `traffic_request`（`amr_behavior_manager` → `amr_agent`）与 `traffic_response`（反向），消息 `TrafficRequest.msg` / `TrafficResponse.msg` 定义在 `amr_interfaces`；**不复用** `task_command` / `task_event`。 | Confirmed |
| D-059 | V2 路权 MQTT topic 固定为 `amr/{robot_id}/traffic_rights/request`（上行）与 `amr/{robot_id}/traffic_rights/response`（下行单播）；Edge 订阅 `amr/+/traffic_rights/request`。动作语义由 payload `op`（`REQUEST` / `CANCEL` / `RELEASE`）承载，**不采用动作分层 topic**。 | Confirmed |
| D-060 | V2 采用**运行时按需触发主模型**：目标全程 `NavigateToPose(D)`，由 AMR **中心点进区边沿**（`point_in_poly(center, polygon)` 的 `false→true`）触发申请；4 点通行控制区沿通道拉长 `L_pull` 自带提前量，**不订阅 Nav2 `plan`、不做提前预判**；授权提前到达则零动作；仅被拒时 cancel 当前导航、等 result 回调后导航到进入侧组内就近等待点、固定 `T_retry = 10 s`（无 jitter）幂等重发、收到响应立即重发 D。保留 Nav2 动态选路能力。 | Confirmed |
| D-061 | V2 释放由**AMR 中心点出区边沿**（`inside` 的 `true→false`）触发，四触发取最早（出区边沿 / 任务结束 `SUCCEEDED`+`ABORTED` / `lease_ttl` 到期 / 显式 `CANCEL`）；`SUCCEEDED` 降级为兜底，与导航 Action 结束解耦。**「改道即释放」保留语义、移除独立 `plan` 机制**：改道后驶离控制区由出区边沿释放，停住或位姿断流由 TTL 回收。 | Confirmed |
| D-062 | V2 路权在 **Edge 中心仲裁**：FIFO 排队、`request_id` 幂等、`epoch` 防陈旧、显式 `cancel` 为一等消息、`lease_ttl` 到期自动回收；不做优先级抢占 / 多资源联合预约 / 死锁检测 / 占用状态广播。 | Confirmed |
| D-063 | V2 窄道数据为**独立 `traffic_gates.yaml`**：每条含 `resource_id`、4 点凸 `polygon`（通行控制区，点序 `P1→P2` 端边 L1 / `P2→P3` 长轴 / `P3→P4` 端边 L2）与 `wait_poses{l1,l2}`（每侧 ≥1）；**只录几何不录路线**，不预求全序列、不下发途经点；不引入 `NavigateThroughPoses` 及独立 `ComputePath` / `FollowPath` Action Client。`MapInfo.msg` 加 `traffic_gates_yaml` 随地图包下发。 | Confirmed |
| D-064 | V2 电量采用 **B + D 方案**：本地按速率积分、无独立充电源；`amr_behavior_manager` 用 `PowerMode` 声明档位，`amr_power_manager` 查表积分、零判定；充电按时间积分。 | Confirmed |
| D-065 | V2 单趟执行编排引入 BehaviorTree.CPP（C 方案），BT 完全位于 `amr_behavior_manager` 内部；不改 `task_state`，不动 V1 的 ACK / 待补报 / 取消语义。 | Confirmed |
| D-066 | V2 的 TBD-020~039 未冻结项（数值参数、表结构、BT 选型、急停落点）不得假定实现方式；需求变更按 10.2 节执行。 | Confirmed |

> **版本口径说明**：早期 SRS 中 FR-019 标注的「V2」泛指后续版本，与 `version-roadmap.md` 的 **V3（Cloud / Web）** 对应；本 1.6 节及 4.10 / 4.11 节的「V2」特指路线图定义的 V2（资源仲裁、行为树、电量、交通路权）。
>
> **V0.20 口径（2026-11-22）**：D-060 / D-061 / D-063 已按 8 项拍板改写，**编号不变**；`D_trigger` 转译为 `L_pull`，旧「只读 `plan` 提前申请 / 越过外移释放线 / 改道即释放不可省」实现描述已废弃。完整定案见 `.workbuddy/artifacts/traffic-gate-8-decisions-frozen.md`，变更记录见 10.4。

详细接口结构、消息字段和幂等规则见 `docs/requirements/interface-contract.md` 和 `docs/architecture/adr/`。

---

## 2. 系统范围

### 2.1 系统组成

| 子系统                    | 主要职责                           |
| ---------------------- | ------------------------------ |
| Cloud Platform         | 目标架构中的上层业务管理、运输任务创建、任务状态查询及云边协同；V1 不交付      |
| Edge Server            | 仓库级服务、AMR 接入、任务下发、地图管理、机器人状态管理 |
| AMR Application        | 机器人初始化、任务接收、行为编排、导航执行、任务状态管理   |
| Simulation Environment | Gazebo 仿真、模拟 AMR 生命周期管理、仿真状态同步 |

### 2.2 系统边界

#### 2.2.1 Cloud Platform

Cloud Platform 是上层业务管理和运输任务创建入口，属于系统目标架构的一部分。

V1 已确认暂不实现 Cloud Platform，改用 Edge 侧任务注入完成运输任务闭环。因此，Cloud Platform 不作为 V1 的交付和验收条件。

后续版本再实现完整的 Cloud Platform，以及完整的 WMS 对接、全局调度算法和 Web 管理界面。

#### 2.2.2 Edge Server

Edge Server 是独立运行的 C++ 服务程序，不依赖 ROS2。

主要职责包括：

* 提供 HTTP API。
* 后续版本通过 Cloud-side MQTT Broker 与 Cloud Platform 通信。
* 通过 Edge-side MQTT Broker 与 AMR 通信。
* 提供 AMR Bootstrap 和注册相关服务。
* 管理地图包的上传、下载、查询及激活。
* 接收、保存和下发运输任务。
* 管理 AMR 的基本信息、连接状态和任务状态。
* 接收 AMR 任务结果，并向 Edge 侧任务注入调用方提供结果查询或状态同步能力。
* 通过 WebSocket 与 Simulation Manager 通信。
* 使用本地数据库和文件系统保存必要的数据。

#### 2.2.3 AMR Application

AMR Application 基于 ROS2 构建，负责机器人自身的业务执行与运动能力。

V1 主要包含：

* `amr_agent`
* `amr_behavior_manager`
* `amr_navigation`

其中：

* `amr_agent` 负责外部通信、初始化、注册、任务接收、状态上报和必要的断网恢复协调。
* `amr_behavior_manager` 负责任务生命周期管理、任务步骤编排、导航调用、取消处理和失败处理。
* `amr_navigation` 负责基于 Nav2 的导航执行。

V2 新增 `amr_power_manager`（独立 ROS2 包）与**内嵌于 `amr_behavior_manager` 的路权逻辑 `TrafficGate`**（不设独立交通模块）；V1 不要求实现上述功能。

除上述业务模块外，V1 工程还包含以下支撑模块：

| 模块 | 用途 |
| --- | --- |
| `amr_common` | AMR 内部共用 Topic 名称等常量。 |
| `amr_interfaces` | AMR 内部 ROS2 消息、服务和 Action 接口定义。 |
| `amr_bringup` | AMR 启动、初始位姿设置等启动辅助能力。 |
| `amr_description` | AMR URDF、传感器、控制器及 RViz/仿真描述。 |
| `amr_simulation` | 面向 AMR 的 Gazebo 仿真与控制器配置。 |
| `warehouse_simulation` | 仓储世界和 Gazebo 启动配置。 |
| `simulation_manager` | 管理仿真 AMR 生命周期、模型及 WebSocket 同步。 |
| `warehouse_tool` | 仓储站点、区域和墙体的可视化编辑工具。 |
| `map_tool` | 地图包打包、校验和上传工具。 |

#### 2.2.4 Simulation Environment

Simulation Environment 由 Simulation Manager 和 Gazebo 组成。

Simulation Manager 负责模拟 AMR 的进程生命周期、仿真模型管理、必要的状态同步和控制器状态检查，并通过 WebSocket 与 Edge Server 通信。

Gazebo 负责提供机器人及仓储环境的物理仿真。

Simulation Environment 不参与 AMR 与 Edge Server 之间的 MQTT 业务通信，也不替代 AMR Application 内部的业务逻辑。

### 2.3 V1 范围

V1 的目标是完成一个可运行、可验证的基本运输任务闭环。

必须具备以下能力：

1. AMR 启动并通过 HTTP 获取 Edge Server 的接入配置。
2. AMR 建立 MQTT 通信并完成注册。
3. AMR 检查本地地图版本，并在必要时同步地图。
4. AMR 完成导航初始化并达到可执行任务的状态。
5. Edge Server 能够通过 V1 任务注入入口创建或导入基本运输任务。
6. Edge Server 能够接收 Edge 侧任务注入创建的任务并向参与 AMR 列表下发。
7. AMR 能够接收任务，并通过 `amr_behavior_manager` 组织任务执行。
8. AMR 能够通过 Nav2 导航前往指定目标点。
9. 系统支持任务取消，并能够反馈取消结果。
10. AMR 与 Edge 通信中断时，允许当前任务在安全条件满足的情况下继续执行。
11. 通信恢复后，AMR 能够补报尚未成功送达的任务状态和结果。
12. 调用方能够通过 Edge Server 获取任务状态及最终执行结果。
13. Simulation Manager 能够通过 WebSocket 与 Edge Server 同步仿真 AMR 状态。
14. 至少两个模拟 AMR 能够使用不同身份标识进行区分。

### 2.4 V1 暂不实现的内容

以下内容不作为 V1 的完成条件：

* 多机器人交通管理和路权协调。
* 复杂的多机器人全局任务分配与优化调度。
* 交通冲突预测、死锁检测和自动恢复。
* 电量消耗模型、自动充电及充电任务调度。
* 完整的 WMS 对接。
* 完整的 Web 管理界面和数据可视化平台。
* 生产级高可用、集群部署及跨仓库容灾。
* 完整的商业级权限管理、审计及安全认证体系。
* 真实机器人硬件驱动、真实传感器集成及实机验证。
* 复杂的机械臂操作任务。

V1 的多 AMR 仿真用于验证实例管理、身份隔离和基本运行能力，不代表系统已具备多机器人交通协调能力。

### 2.5 假设与依赖

V1 基于以下假设进行设计：

1. V1 的验证环境为 Gazebo 仿真，不连接真实 AMR、真实传感器或真实仓库设备。
2. V1 暂不实现 Cloud Platform；任务通过 Edge 侧 HTTP API 任务注入入口创建。
3. Edge Server 与 AMR 处于可互通的网络中，Edge-side EMQX 可被访问；Cloud-side EMQX 属于后续版本。
4. V1 以单个仓库 `warehouse_01` 和单个 Edge Server 为主要验证范围。
5. AMR 内部多个 ROS2 节点运行在同一仿真机器人命名空间下，并使用相对 Topic/Service/Action 名称。
6. 安全策略在仿真环境中重点验证控制流和状态流，不涉及真实急停、硬件保护或人员安全认证。
7. 若需要将系统迁移到真实机器人，必须重新评估定位、传感器标定、控制器参数、安全停靠和硬件通信需求。

关键外部依赖至少包括：

* Ubuntu 22.04。
* ROS2 Humble。
* Nav2。
* Gazebo 11 及 `gazebo_ros`。
* EMQX 或其他兼容 MQTT Broker。
* C++17 编译工具链、CMake。
* Paho MQTT C++、yaml-cpp、libwebsockets、SQLite。
* Python 3 及地图工具相关依赖。

上述依赖版本、安装方式和启动命令应由构建与部署文档维护。

---

## 3. 系统架构与通信约束

### 3.1 子系统通信关系

| 通信双方                             | 通信方式                          | 主要用途                 |
| -------------------------------- | ----------------------------- | -------------------- |
| Cloud Platform ↔ Edge Server     | HTTP                          | 后续版本：API 调用及业务数据交互        |
| Cloud Platform ↔ Edge Server     | MQTT                          | 后续版本：任务创建请求、业务事件及状态交互     |
| Edge Server ↔ AMR                | HTTP                          | Bootstrap、地图包下载等请求   |
| Edge Server ↔ AMR                | MQTT                          | 注册、任务下发、任务取消、状态及业务消息 |
| Simulation Manager ↔ Edge Server | WebSocket                     | 仿真机器人状态及生命周期同步       |
| AMR 内部模块                         | ROS2 Topic / Service / Action | 状态发布、请求调用和长时间运行任务执行  |
| Simulation Manager ↔ Gazebo      | Gazebo 相关接口及进程管理              | 仿真模型、仿真进程及运行状态管理     |

具体业务采用 HTTP 还是 MQTT，应在接口设计文档中明确，不要求同一业务同时通过两种方式实现。

### 3.2 MQTT 基础设施

系统存在两套相互独立的 MQTT Broker：

1. **Cloud-side EMQX**：后续版本用于 Cloud Platform 与 Edge Server 之间的 MQTT 通信；V1 不接入。
2. **Edge-side EMQX**：用于 Edge Server 与 AMR 之间的 MQTT 通信。

两套 Broker 在逻辑和部署上独立，不应在架构设计中合并为同一个 MQTT Broker。

### 3.3 架构约束

1. Edge Server 不依赖 ROS2，不通过 ROS2 Topic、Service 或 Action 与 AMR 通信。
2. AMR 内部模块使用 ROS2 进行通信。
3. `amr_agent` 负责 AMR 对外接入和主要业务通信。
4. Simulation Manager 与 Edge Server 使用 WebSocket，不使用 MQTT 代替这条通信链路。
5. Simulation Environment 与 AMR 的业务执行逻辑保持解耦。
6. V1 不实现交通管理，但应避免将导航模块与未来交通协调逻辑强耦合。
7. 模块之间通过明确的接口交互，避免直接依赖其他模块的内部实现。
8. 新增需求不得擅自改变上述架构约束；确需调整时，应先更新相关设计文档并确认影响范围。

### 3.4 关键接口与消息概览

本节给出 V1 当前需要覆盖的接口范围。详细请求/响应结构、错误码和状态机由接口设计文档定义。

#### 3.4.1 Edge Server HTTP API

| 方法 | 路径 | 主要用途 |
| --- | --- | --- |
| GET | `/api/amr/bootstrap` | AMR 获取仓库标识、Edge-side MQTT 连接参数。 |
| POST | `/api/maps/upload` | 上传地图包，可指定是否上传后立即激活。 |
| GET | `/api/maps/download` | 下载当前生效地图包。 |
| GET | `/api/maps/active` | 查询当前生效地图元数据。 |
| POST | `/api/maps/activate` | 激活指定版本地图。 |
| POST | `/api/tasks` | V1 Edge 侧任务注入。 |
| GET | `/api/tasks/{task_id}` | 查询任务状态。 |
| POST | `/api/tasks/{task_id}/cancel` | 取消任务。 |

V1 通过 Edge 侧 HTTP API 创建任务。后续 Cloud Platform 保留该接口，并可通过 MQTT 发布任务。

#### 3.4.2 Edge Server 与 AMR 的 MQTT Topic

| Topic 模式 | 方向 | 主要用途 |
| --- | --- | --- |
| `edge_server/status` | Edge → AMR | 发布 Edge Server 在线状态。 |
| `amr/register/request` | AMR → Edge | AMR 注册请求。 |
| `amr/register/response/{robot_id}` | Edge → AMR | 指定机器人的注册响应。 |
| `amr/{robot_id}/status` | AMR → Edge | AMR 运行状态、心跳和任务状态快照。 |
| `amr/{robot_id}/will` | AMR → Edge | MQTT Last Will，用于标识 AMR 异常离线。 |
| `warehouse/map/request` | AMR → Edge | 地图请求，预留接口。 |
| `amr/{robot_id}/traffic_rights/request` | AMR → Edge | **V2 已定名**：路权申请 / 显式取消 / 释放，payload `op` = `REQUEST` / `CANCEL` / `RELEASE`，QoS 1。 |
| `amr/{robot_id}/traffic_rights/response` | Edge → AMR | **V2 已定名**：路权响应，单播不广播，QoS 1。 |

Cloud → Edge 任务 Topic 使用 `warehouse/{warehouse_id}/task`，Edge → Cloud 任务状态 Topic 使用 `warehouse/{warehouse_id}/task/status`，QoS 1。

Edge → AMR 任务 Topic 使用 `amr/{robot_id}/task`，任务级共享状态 Topic 使用 `warehouse/{warehouse_id}/task/{task_id}/state`，QoS 1。任务下发不包含按 AMR 平均分配的数量。

AMR 搬运申请使用 `amr/{robot_id}/task/goods/request`，Edge 响应使用 `amr/{robot_id}/task/goods/response`，QoS 1。

V2 路权 Topic 由 Edge 以 `amr/+/traffic_rights/request` 通配订阅（`robot_id` 一层通配，新增机器人零配置）；V1 常量 `TRAFFIC_RIGHTS_REQ` / `TRAFFIC_RIGHTS_RSP_PREFIX` 缺 `robot_id` 层，V2 实现时须替换为 `AMR_TRAFFIC_REQUEST` / `AMR_TRAFFIC_RESPONSE` / `AMR_TRAFFIC_SUBSCRIBE`。消息字段与 JSON 编码见 `docs/requirements/interface-contract.md` 第 8 节。

#### 3.4.3 AMR 内部 ROS2 接口

| 类型 | 名称 | 方向 | 主要用途 |
| --- | --- | --- | --- |
| Topic | `map_info` | `amr_agent` → 其他 AMR 节点 | 发布当前地图信息。 |
| Topic | `robot_status` | `amr_behavior_manager` → 其他 AMR 节点 | 发布机器人聚合状态。 |
| Topic | `task_status` | `amr_behavior_manager` → 其他 AMR 节点 | 发布当前任务状态快照。 |
| Topic | `robot_pose` | `amr_navigation` → 其他 AMR 节点 | 发布当前位姿。 |
| Topic | `navigation_status` | `amr_navigation` → `amr_agent` | 发布导航模块统一状态。 |
| Topic | `behavior_status` | `amr_behavior_manager` → `amr_agent` | 发布行为编排模块统一状态。 |
| Action | `amr_navigate_to_pose` | `amr_behavior_manager` → `amr_navigation` | 提交单点导航目标并获取执行结果。 |
| Service | `get_robot_pose` | `amr_behavior_manager` → `amr_navigation` | 按需查询机器人位姿。 |

所有 AMR 内部接口优先使用相对名称，在机器人命名空间下解析，避免多 AMR 之间的 Topic、节点和 TF 冲突。

#### 3.4.4 V2 新增 AMR 内部 ROS2 接口（已定名）

| 类型 | 名称 | 常量 | 方向 | 主要用途 |
| --- | --- | --- | --- | --- |
| Topic | `traffic_request` | `amr_common::topic::TRAFFIC_REQUEST` | `amr_behavior_manager` → `amr_agent` | 路权申请 / 显式取消 / 释放（`op` 三态）。 |
| Topic | `traffic_response` | `amr_common::topic::TRAFFIC_RESPONSE` | `amr_agent` → `amr_behavior_manager` | 路权响应（`GRANTED` / `REJECTED` / `PENDING` 等）。 |
| Topic | `power_mode` | `amr_common::topic::POWER_MODE` | `amr_behavior_manager` → `amr_power_manager` | 声明 IDLE / WORK / CHARGING 档位（D 方案）。 |
| Topic | `power_level` | `amr_common::topic::POWER_LEVEL` | `amr_power_manager` → `amr_behavior_manager` | 发布 SOC 与低电量标志。 |

新增消息 `TrafficRequest.msg`、`TrafficResponse.msg`、`PowerMode.msg`、`BatteryState.msg` 定义在 `amr_interfaces`；字段表见 `docs/architecture/roadmap/v2-implementation-spec.md` 第 2 节。**V2 不新增、不修改 `TaskCommand.msg` / `TaskEvent.msg`**（D-058）。

**V0.20 补充**：V1 已有实体文件 `MapInfo.msg`（`map_info` topic，含 `map_yaml` / `zones_yaml` / `stations_yaml`）新增 `string traffic_gates_yaml` 字段，用于随地图包下发独立的 `traffic_gates.yaml`；该字段与打包 / 下发 8 处链路、数据结构及 `validator.py` 内容校验见 `docs/requirements/interface-contract.md` 第 8 节。上述 4 个 V2 路权 / 电量 `.msg` 目前尚无实体文件，仅以文本定义于接口契约第 8 节与实现规格第 2 节，V2 实现时创建；字段集合保持不变（`TrafficResponse` 已有 `queue_seq`）。

---

## 4. 功能需求

### 4.1 AMR 启动与接入

#### FR-001：AMR Bootstrap

**优先级：** Must
**版本：** V1

AMR 启动后，`amr_agent` 应能够通过 HTTP 请求 Edge Server 的 Bootstrap API。

请求至少提供 AMR 的身份标识 `robot_id`。

在 AMR 身份及配置有效的情况下，Edge Server 应返回接入所需的基础配置，至少包括：

* `warehouse_id`
* Edge-side MQTT Broker 地址及端口
* MQTT 连接所需的配置参数

当前生效地图版本或下载地址可在注册响应中返回，而不是要求 Bootstrap 接口一次性包含全部地图信息。若后续实现调整，应同步更新接口设计文档。

请求参数无效、AMR 未注册或服务不可用时，应返回可识别的错误信息。

具体请求字段、响应结构和错误码在接口设计文档中定义。

#### FR-002：AMR 注册

**优先级：** Must
**版本：** V1

AMR 获取接入配置后，应能够连接 Edge-side MQTT Broker，并完成机器人注册。

注册消息应包含识别 AMR 所需的基本信息，至少能够关联：

* `robot_id`
* 所属仓库
* `instance_id`（仿真场景下需要区分实例）
* 当前运行状态或必要的初始化信息
* 本地地图版本或为空的地图版本信息

Edge Server 应记录注册结果，并维护已接入 AMR 的基本信息。

注册响应应至少包含 `code`、`message`、`robot_id`、`request_id`。当需要同步地图时，还应包含目标 `map_version` 和 `map_download_url`。Edge 还应返回分配给该 AMR 的初始点位 `initial_pose`（`x`、`y`、`yaw`），V1 由 IDLE 充电点按顺序分配得到。

注册失败时，AMR 应能够识别失败状态，并根据错误类型进行重试或等待处理。

#### FR-003：通信重连与状态恢复

**优先级：** Must
**版本：** V1

当 AMR 与 Edge Server 之间的 HTTP 或 MQTT 通信暂时中断时，系统应能够识别通信异常。

对于 MQTT 连接中断，AMR 应能够重新连接，并在连接恢复后重新订阅必要的消息。

通信恢复后，AMR 应能够重新同步必要的机器人状态、当前任务状态及尚未成功送达的任务结果。

对于尚未完成的任务，系统应能够识别任务的实际执行状态，避免仅因通信中断而重复执行同一任务。

本需求要求实现基本的重连与结果补报能力，不要求 V1 实现复杂的分布式事务或完整的消息中间件持久化方案。

### 4.2 地图管理与同步

#### FR-004：地图包管理

**优先级：** Must
**版本：** V1

Edge Server 应支持地图包的上传、下载、查询及激活。

地图包应能够关联仓库标识和地图版本。

Edge Server 应能够识别当前生效的地图版本，并保存地图包的必要元数据。

地图包格式、压缩格式、版本命名规则和校验方式在地图接口设计中定义。

#### FR-005：AMR 地图版本检查与同步

**优先级：** Must
**版本：** V1

AMR 启动并获得仓库相关配置后，应能够检查本地地图与 Edge Server 当前生效地图之间的版本关系。V1 可通过注册响应中的 `map_version`、`map_update` 和 `map_download_url` 完成该判断。

当本地地图缺失或版本不一致时，AMR 应能够通过 HTTP 下载所需地图包。

下载完成后，AMR 应对地图包进行必要的完整性检查，并将其保存到约定的本地地图目录。

只有在地图包下载及必要的校验成功后，才应将其视为可用地图。

地图下载失败时，不应将不完整的地图包直接作为有效地图使用。

地图下载、校验、切换和 Nav2 加载之间的详细流程在设计文档中定义。

### 4.3 AMR 导航初始化

#### FR-006：导航初始化

**优先级：** Must
**版本：** V1

AMR 应能够在地图准备完成后完成导航所需的初始化。

初始化至少涉及：

* 地图文件及导航配置准备。
* Nav2 相关节点启动和就绪检查。
* 定位初始化及初始位姿设置。
* 必要的 TF 和导航状态检查。

导航系统未就绪时，不应将 AMR 标记为可执行导航任务。

导航初始化的执行主体、节点启动时序及错误恢复策略在 AMR 设计文档中定义。

### 4.4 Edge 任务管理（V1）与 Cloud 任务管理（后续版本）

#### FR-007：Edge 侧任务注入（V1 替代 Cloud 创建）

**优先级：** Must
**版本：** V1

V1 已确认暂不实现 Cloud Platform。运输任务由 Edge 侧 HTTP API 任务注入入口创建或导入，Edge Server 负责保存和后续下发。

该入口必须能够创建至少包含以下信息的运输任务：

* 唯一任务标识 `task_id`
* 参与任务的 AMR 标识列表 `robot_ids`
* 任务类型
* 目标位置或导航目标点
* 必要的任务参数

Edge Server 应能够识别同一个任务，避免重复创建或无法关联的任务记录。

V1 不要求实现复杂的自动任务分配算法。参与 AMR 的选择方式和异常处理规则在接口设计中明确。

#### FR-008：Edge 任务接收与下发

**优先级：** Must
**版本：** V1

Edge Server 应能够接收 Edge 侧任务注入入口创建或导入的任务，校验必要的任务参数，并将任务下发至 `robot_ids` 中的参与 AMR。

Edge Server 应维护任务的基本状态，并能够识别任务是否已下发、是否已被 AMR 接受，以及当前执行结果。

当参与 AMR 不可用、任务参数无效或下发失败时，Edge Server 应能够记录失败原因，并向任务注入调用方提供可识别的处理结果。

Edge Server 不应将“消息已发送”等同于“任务已成功执行”。

### 4.5 AMR 任务执行

#### FR-009：任务接收与执行编排

**优先级：** Must
**版本：** V1

`amr_agent` 接收到任务消息后，应对消息进行必要的格式及参数校验，并将有效任务交给 `amr_behavior_manager` 处理。

`amr_behavior_manager` 负责：

* 管理任务执行生命周期。
* 按照既定流程组织任务步骤。
* 调用导航能力执行目标点移动。
* 处理导航成功、失败及取消等结果。
* 根据执行结果更新任务状态。
* 执行必要的失败处理。
* 处理任务执行期间的取消请求。

行为树或等价的状态机/流程编排机制可用于实现任务步骤编排、条件判断和失败分支。

`amr_behavior_manager` 不负责替代 Nav2 的路径规划与运动控制，也不直接承担上层调用方的任务创建或 Edge 的任务下发职责。

#### FR-010：导航目标执行

**优先级：** Must
**版本：** V1

`amr_navigation` 应基于 Nav2 提供导航执行能力。

`amr_behavior_manager` 应能够通过明确的 ROS2 接口向 `amr_navigation` 提交目标位置，并获得导航执行结果。

导航模块应能够区分至少以下结果：

* 导航成功。
* 导航失败。
* 导航被取消。

当导航失败时，`amr_behavior_manager` 应能够依据返回结果决定任务后续处理方式。

V1 只要求基本导航，不要求实现交通冲突检测、路权申请、交通等待和多机器人协同路径规划。

#### FR-011：任务状态管理

**优先级：** Must
**版本：** V1

系统应维护基本的任务状态。

建议的状态集合如下：

| 状态          | 含义        |
| ----------- | --------- |
| `PENDING`   | 任务等待下发或执行 |
| `ACCEPTED`  | AMR 已接受任务 |
| `RUNNING`   | 任务正在执行    |
| `SUCCEEDED` | 任务成功完成    |
| `FAILED`    | 任务执行失败    |
| `CANCELLED` | 任务已取消     |
| `INTERRUPTED` | 任务因全部 AMR 异常等原因中断 |

状态转换应受到约束，不应出现无法解释的状态跳转。

V1 至少应支持以下基本状态流转：

* 正常执行：`PENDING` → `ACCEPTED` → `RUNNING` → `SUCCEEDED`
* 执行失败：`RUNNING` → `FAILED`
* 执行取消：任务进入执行阶段后，收到有效取消请求，完成必要的取消处理后进入 `CANCELLED`
* 执行中断：参与任务的全部 AMR 均异常时，`RUNNING` → `INTERRUPTED`

系统应区分任务的业务状态与 MQTT 消息的发送状态。通信失败不得直接将业务任务标记为失败。

任务取消后的状态一致性、重复请求处理及终态任务的重新执行规则，应在接口设计中进一步明确。

#### FR-012：任务取消

**优先级：** Must
**版本：** V1

系统应支持对已接受或正在执行的运输任务发起取消请求。

取消请求应能够从上层业务经 Edge Server 传递至参与任务的 AMR。

AMR 收到有效取消请求后，应由 `amr_behavior_manager` 协调取消当前任务，并在适当情况下通过导航接口取消正在执行的导航动作。

取消处理应满足以下要求：

1. 取消请求能够关联正确的 `task_id`。
2. 不应因重复收到同一取消请求而重复执行取消操作。
3. 系统应区分“取消请求已收到”和“任务已成功取消”。
4. 只有在任务取消处理完成后，才能将任务状态更新为 `CANCELLED`。
5. AMR 应将取消结果上报 Edge Server。
6. Edge Server 应更新任务状态，并使任务注入调用方能够获取取消结果。
7. 如果任务已经成功完成或已经失败，应根据既定状态规则处理取消请求，不得错误地覆盖原有终态。

任务取消应优先通过正常的任务控制流程实现，不得以直接终止 ROS2 节点或强制结束机器人进程作为常规取消方式。

任务取消时的安全停止策略及具体状态转换，应在 `amr_behavior_manager` 和导航接口设计中定义。

### 4.6 通信中断与任务连续性

#### FR-013：通信中断期间继续执行当前任务

**优先级：** Must
**版本：** V1

当 AMR 与 Edge Server 之间的 MQTT 通信中断时，如果当前任务已经被 AMR 接受并开始执行，且机器人自身运行条件、安全状态和任务条件允许，则 AMR 可以继续执行当前任务。

通信中断本身不得自动导致正在执行的任务失败或被取消。

AMR 应在本地维护当前任务的必要状态，以便在通信恢复后继续进行状态同步。

本需求不代表机器人可以忽略安全故障、导航故障或其他必须停止执行的异常。机器人自身的安全和故障处理优先于任务连续性。

V1 不要求在通信中断期间继续接收和执行新的远程任务。

#### FR-014：通信恢复后的任务状态补报

**优先级：** Must
**版本：** V1

当 AMR 与 Edge Server 的 MQTT 通信恢复后，AMR 应能够重新同步当前任务状态及尚未成功送达的任务结果。

至少应满足：

1. AMR 能够识别通信中断期间发生的任务状态变化。
2. AMR 能够补报任务成功、失败或取消等最终结果。
3. 补报消息应包含能够关联任务和机器人的标识。
4. Edge Server 应能够处理重复收到的状态消息，不得因重复消息重复创建任务或重复执行任务。
5. Edge Server 应更新任务的最新有效状态。
6. 任务注入调用方应能够通过 Edge Server 获取最终任务结果。

V1 可以采用轻量级的本地待发送结果记录和重连后补报机制，不要求实现完整的分布式事务系统。

具体的消息确认、补报顺序、记录清理和状态冲突处理策略在接口设计文档中定义。

### 4.7 机器人状态管理

#### FR-015：AMR 状态上报

**优先级：** Must
**版本：** V1

AMR 应能够向 Edge Server 上报基本运行状态。

状态信息至少应支持识别：

* `robot_id`
* 当前连接状态或最后已知连接状态
* 当前任务标识及任务状态（如有）
* 必要的状态更新时间

Edge Server 应能够区分不同 AMR，并维护最新的有效状态。

对于位置、电量、故障码等扩展状态，可根据 V1 实际实现情况逐步补充。

### 4.8 仿真管理

#### FR-016：仿真 AMR 生命周期管理

**优先级：** Must
**版本：** V1

Simulation Manager 应能够管理仿真环境中的 AMR 实例，包括启动、运行状态检查及必要的生命周期管理。

每个仿真 AMR 应具有可区分的身份信息，包括：

* `robot_id`
* `instance_id`

Gazebo 中的模型名称应能够根据机器人身份进行区分，避免多个仿真实例使用相同模型名称造成冲突。

#### FR-017：仿真状态同步

**优先级：** Must
**版本：** V1

Simulation Manager 应通过 WebSocket 与 Edge Server 交换仿真 AMR 相关状态。

系统应能够识别仿真 AMR 的新增、运行状态变化及必要的生命周期事件。

仿真状态同步不应依赖 AMR 与 Edge Server 之间的 MQTT 业务链路。

#### FR-018：多 AMR 仿真

**优先级：** Must
**版本：** V1

系统应至少支持两个模拟 AMR 在同一仿真环境中使用不同身份运行。

不同 AMR 应具有相互隔离的 ROS2 命名空间和必要的运行配置，以避免 Topic、节点、TF 和控制器资源发生意外冲突。

V1 验收重点是多 AMR 的身份区分、基本启动和导航能力，不要求实现多机器人交通协调。

### 4.9 Cloud 与 Edge 协同（后续版本）

#### FR-019：Cloud 与 Edge 通信

**优先级：** Should
**版本：** V2

V1 已确认暂不实现 Cloud Platform，因此本需求不作为 V1 完成条件。

后续版本中，Cloud Platform 应能够通过 HTTP 与 Edge Server 进行必要的 API 交互，并通过 Cloud-side MQTT Broker 与 Edge Server 交换业务消息。

具体哪些交互采用 HTTP、哪些采用 MQTT，应在接口设计文档中确定。

#### FR-020：端到端业务闭环

**优先级：** Must
**版本：** V1

V1 应能够验证从 Edge 侧任务注入到 AMR 执行任务并反馈结果的完整业务流程。

正常执行流程至少包括：

1. 通过 Edge 侧任务注入入口创建或导入运输任务。
2. Edge Server 校验并保存任务。
3. Edge Server 将任务下发给 `robot_ids` 中的参与 AMR。
4. AMR 接收并接受任务。
5. `amr_behavior_manager` 组织任务执行。
6. `amr_navigation` 调用 Nav2 前往目标位置。
7. AMR 获得导航结果并更新任务状态。
8. AMR 将任务状态上报 Edge Server。
9. Edge Server 更新任务状态。
10. 通过 Edge 侧查询或日志获取任务最终结果。

取消流程至少包括：

1. Edge 侧任务注入入口或其他获授权的调用方发起任务取消请求。
2. 请求经 Edge Server 传递至参与任务的 AMR。
3. AMR 协调取消任务及必要的导航动作。
4. AMR 将取消结果反馈 Edge Server。
5. Edge Server 更新任务状态。
6. 调用方获取取消结果。

通信中断恢复流程至少包括：

1. AMR 与 Edge 的 MQTT 通信中断。
2. AMR 在满足安全条件的前提下继续执行当前任务。
3. AMR 本地记录必要的任务状态变化及待补报结果。
4. MQTT 通信恢复。
5. AMR 重新同步状态并补报结果。
6. Edge Server 更新有效任务状态。
7. 调用方通过 Edge Server 获取最终结果。

### 4.10 V2 交通路权与资源仲裁

#### FR-021：运行时按需触发的路权申请与导航连续性

**优先级：** Must
**版本：** V2

系统应在一趟任务可能经过多个窄道时，**不预先决定走哪条路**：路权挂在 4 点通行控制区上，由 AMR **中心点进区边沿**（`point_in_poly(center, polygon)` 的 `inside` 由 `false→true`）触发申请；区域沿通道拉长 `L_pull` 自带提前量，**不订阅 Nav2 `plan`、不做提前预判**（旧 `D_trigger` /「只读 `plan` 提前预判」口径已废弃）。方向无关，掉头从入口退出天然触发出区。

1. 任务目标全程为最终目的地 D（`NavigateToPose(D)`），由 Nav2 动态选路；不预先求全序列、不录路线顺序、不下发途经点、不引入 `NavigateThroughPoses` 及独立 `ComputePath` / `FollowPath` Action Client。
2. 授权提前到达时系统应**零动作**：`REQUESTING` 收 `GRANTED` 不减速、不取消导航、不重发 goal、不切分任务（AC-020）；只有 `WAITING` 获批才立即 `navigateToPose(D)`。
3. 申请被拒或超时时，AMR 应取消当前导航、**等 result 回调 `m_goal_active=false` 后**导航到**进入侧端边外侧（上游）组内就近**的等待点并停稳；等待期间 Nav2 无活动 goal、`cmd_vel` 归零，以规避 `SimpleProgressChecker` 的 20 s 超时。
4. 等待期间以固定 `T_retry = 10 s`（无 jitter）用同一 `request_id` 幂等重发；收到响应应**立即**重发 `NavigateToPose(D)`，不等轮询。
5. 一趟任务全程只有一个 `task_state`；路权闸门不得切分任务、不得产生 `INTERRUPTED` 或重复 `CARRY_*` 上报。
6. `amr_navigation` 的 4 个已知缺陷（无条件 `ACCEPT_AND_EXECUTE` 的 supersede、60 s 结果超时只 abort 不 cancel Nav2 goal、`m_nav_epoch` 竞态、`accepted` 语义混淆）为**实施前置阻塞项**，接入路权前必须先修复。

#### FR-022：Edge 侧中心仲裁与 lease 生命周期

**优先级：** Must
**版本：** V2

Edge Server 应作为唯一持有全局 lease 视图的一方，对 `TRAFFIC_RIGHT` / `GOODS_QUOTA` / `CHARGE_POINT` 提供统一仲裁：

1. 同一资源同一时刻只能有一个 holder；`PENDING`（排队中）不占资源，按 FIFO `queue_seq` 依次授权。
2. 请求以 `request_id` 幂等去重；以 `epoch` 丢弃迟到陈旧消息；`robot_id` 须与 MQTT topic 中的 `{robot_id}` 交叉校验。
3. **取消必须是一等显式消息**（payload `op = CANCEL`），不得靠「不再重试 / 断线 / 静默」推断；排队中取消 = 出队，已授权取消 = 释放 lease 并回退配额。
4. 申请携带 `lease_ttl`（权限占用时限），授权时写入 `lease_expires_at`；到期、心跳超时、Last Will 触发或取消消息丢失时自动回收，不得出现资源永久被锁。
5. Edge 重启后应能从 SQLite 重建占用状态与 `PENDING` 队列（或显式作废），不得留下幽灵排队项。
6. 响应走单播，路权占用状态**不得广播给其他 AMR**。
7. 复杂度边界：不做优先级抢占、多资源联合预约、死锁检测与回滚、AMR 侧排队状态广播。

#### FR-023：路权释放与导航结果解耦

**优先级：** Must
**版本：** V2

1. 释放由**AMR 中心点出区边沿**（`inside` 的 `true→false`，配合边沿检测 + 滞回双阈值）的空间事件触发，与导航 Action 的返回结果解耦；车不停、不改 goal。
2. 四个触发取**最早者**：① 出区边沿；② 导航 `SUCCEEDED` / 任务结束 / 异常 `ABORTED`；③ `lease_ttl` 到期由 Edge 自动回收；④ 显式 `op=CANCEL`。
3. **「改道即释放」保留语义、移除独立机制**：授权后改选绕行路的车驶离通行控制区时由「出区边沿」释放；停住区内 / 位姿断流由 `lease_ttl` 回收；不设 `onPlanChanged()` / `pathIntersects*` / 位姿远离判定 / 改道去抖，也不订阅 `plan`。
4. 主判据只用中心点 `point_in_poly`，不做逐帧 `footprint` / 车尾判定；由 `min_release_clearance` 把「中心过端边 ⇒ 车尾已出窄道」变成录入时的一次性静态几何约束；`crossed()` 已废弃为主判据，仅保留作侧边异常穿入识别辅助。

#### FR-024：窄道几何数据模型与模块归属

**优先级：** Must
**版本：** V2

1. 窄道数据为**独立 `traffic_gates.yaml`**，不写回 `zones.yaml` / `stations.yaml`：每条含 `resource_id`、4 点凸 `polygon`（点序冻结：`P1→P2` 端边 L1 / `P2→P3` 长轴 / `P3→P4` 端边 L2）与 `wait_poses{l1,l2}`（每侧 ≥1 个，位于入口端边外侧 / 上游）；**无 `side` / `forward` / `trigger` / `release` 字段**，只录几何不录路线。不新建 station、不复用 `WAIT`、不需要 `TRAFFIC_WAIT` 类型；`zone.type` 扩展 `NARROW`、窄道 zone 下挂 station、成对有向触发段 / 外移释放线与 `side` 的 V0.19 口径**已废弃**。
2. 几何 = **端边外移 + 侧边拉长的通行控制区**，不是 3×2 物理窄道：端边外移 `min_release_clearance`，侧边沿通道拉长 `L_pull ≥ v_max×(RTT_拒绝_p99 + T_cancel + T_newgoal_accept) + v_max²/(2·abs(a_dec)) + d_safety`（数值留 TBD-021 / 038 待 V1.5 实测）；端边不压分叉点，保走绕行路的车「从不进区」→ 零申请（S6）。旧 `D_trigger` 已废弃，其提前量语义由 `L_pull` 转译承接。
3. 主判据 = **AMR 中心点 `point_in_poly(center, polygon)` 的 `inside` 边沿**：进区 `false→true` 发 `REQUEST`，出区 `true→false` 发 `RELEASE`；数据源为 `amr_navigate_to_pose` action feedback 的 `current_pose`，**1 Hz `robot_pose` 禁止作主判据**；必须**边沿检测 + 滞回双阈值**（内缩进入 / 外扩离开，TBD-038）+ 状态过滤。方向无关，掉头从入口退出天然触发出区；`crossed()` 已废弃为主判据，仅作侧边异常穿入识别辅助（按 `s` 兜底 + `LOG_WARN`）。
4. 进入侧算法：边沿时刻按 `long_axis=normalize(P3-P2)`、`centroid=mean(P1..P4)`、`s=dot(enter_pose-centroid, long_axis)` 定侧（`s<0→L1/wait_poses.l1`，`s>0→L2/wait_poses.l2`），组内 `argmin dist` 就近选点；**`entry_side` 在本次穿越周期锁存，不得每帧重算**。多车默认组内 `argmin dist` + validator 点间距校验，备选 `queue_seq % N_side`（`TrafficResponse` 已有 `queue_seq`，不新增字段）。不订阅 `plan`、不引入 `NavigateThroughPoses` / 独立 `ComputePath` / `FollowPath`。
5. 工具侧新增 `traffic_gates_editor` **两阶段**（阶段 A 录 4 点 zone、阶段 B 录 N 个等待点并按几何自动归档到 `l1` / `l2`），复用 `ClickedPointReceiver` / `ZoneMarkerPublisher`；**不复用** `zone_editor` 节点本体 / `YamlZoneManager` / `ZoneInfo` / `selectZoneType()`，不扩展 zone / station type 菜单。`map_tool/validator.py` 现状只查文件存在，须新增 7 条内容校验：① `polygon` 恰为 4 点凸四边形、点序连续、面积 > 0；② 两条端边近似平行且与长轴近似垂直、间距 `L_pull` 达下限；③ 端边与 `walls.yaml` 不冲突且通道宽度 ≥ 车宽 + 余量；④ `wait_poses.l1` / `.l2` 各 ≥1、在对应端边外侧且不在 `polygon` 内、同侧相邻点间距 ≥ 车宽 + `d_queue_spacing`；⑤ 端边到真实窄道出口净距 ≥ `min_release_clearance`；⑥ 区域不与分叉点之前的公共通道重叠（保 S6 零误触发）；⑦ `resource_id` 全局唯一非空。
6. `MapInfo.msg` 新增 `traffic_gates_yaml` 字段，`traffic_gates.yaml` 作为地图包第 4 个文件随包下发（8 处链路：`REQUIRED_WAREHOUSE_FILES` / `metadata.py` / `package.py` / `validator.py` / `MapInfo.msg` / `map_manager.cpp::publishMapInfo()` / `amr_behavior_manager` MapInfo 回调 / 新 `traffic_gate_loader`）；完整数据结构与字段表见 `docs/requirements/interface-contract.md` 第 8 节。
7. **V2 路权逻辑不设独立模块**，实现为 `amr_behavior_manager` 包内 `TrafficGate`；`amr_agent` 只补 `TrafficBridge` 透明桥，不持有位姿 / 几何 / 业务决策（INV-14 模块归属不变）。
8. ROS2 / MQTT topic 命名按 D-058 / D-059 与 INV-14 固定，实现者不得自行选择。

### 4.11 V2 电量、行为树与安全

#### FR-025：电量消耗与自动充电模拟

**优先级：** Must
**版本：** V2

1. `amr_power_manager` 为 AMR 侧 SOC 真值唯一持有者；Edge 侧只保存 battery 只读镜像，不得产生、累加或修改 SOC 数值。
2. `amr_behavior_manager` 用专用、单向、不含 `battery` 的 `PowerMode` 声明 IDLE / WORK / CHARGING 档位；`amr_power_manager` 查表积分、零运行时判定、不订阅 `cmd_vel`。
3. 充电采用本地按时间速率积分（B 方案），不引入独立充电模拟器、无远端充电源、无 `ChargeSource` 抽象、不实现 CC-CV。
4. 低电量应触发充电准入与 `CHARGE_POINT` 资源申请；battery 数值不再硬编码为 `100.0`。

#### FR-026：行为树编排与软件急停

**优先级：** Should
**版本：** V2

1. 单趟执行编排引入 BehaviorTree.CPP（C 方案），实现 `Retry` / `Fallback` / `RequestOnApproach` / `WaitAndRetry` / `ReleaseTrafficRight` 等节点；BT 完全位于 `amr_behavior_manager` 内部，不改 `task_state`。
2. BT 技术选型、XML 路径、自定义节点注册方式由 TBD-023 冻结；不得直接照抄文档示意。
3. 多机与交通路权同时存在时应提供软件急停与 `cmd_vel` 优先级仲裁；触发源与仲裁落点由 TBD-024 冻结。

---

## 5. 非功能需求

### NFR-001：模块化与单一职责

**优先级：** Must

各模块应具有清晰的职责边界。

业务编排、导航执行、外部通信、仿真管理和数据持久化不应无序耦合。

模块之间应通过明确的接口进行交互，避免依赖其他模块的内部实现细节。

### NFR-002：通信隔离

**优先级：** Must

AMR 内部使用 ROS2；AMR 与 Edge Server 使用 HTTP 和 MQTT；Simulation Manager 与 Edge Server 使用 WebSocket。

不得为了简化实现而破坏已确定的通信边界。

### NFR-003：可构建性

**优先级：** Must

项目应能够在约定的 Ubuntu 22.04 开发环境中构建。

ROS2 工作空间应能够使用 `colcon build` 构建；独立 C++ 的 Edge Server 应能够使用 CMake 构建。

必要的依赖、构建命令和环境要求应有文档记录。

### NFR-004：可测试性

**优先级：** Must

核心模块应能够进行独立测试或通过可重复的集成流程验证。

至少应能够验证：

* HTTP 接口。
* MQTT 消息交互。
* 地图包管理与同步。
* AMR 导航执行。
* 任务状态流转。
* 任务取消。
* 通信恢复及任务结果补报。
* WebSocket 仿真状态同步。
* 多 AMR 身份隔离。

### NFR-005：故障可诊断性

**优先级：** Must

系统应对启动失败、通信异常、地图同步失败、导航失败、任务取消失败和任务执行失败提供必要的日志信息。

日志应包含足以定位问题的模块标识、任务标识、机器人标识和错误原因。

### NFR-006：可维护性

**优先级：** Must

C++ 模块应具有合理的类职责、接口边界和依赖关系。

重要的构建配置、运行配置及第三方依赖应有明确的管理方式。

### NFR-007：可扩展性

**优先级：** Should

架构应为后续增加交通管理、电量管理、复杂任务调度和 WMS 对接预留合理的扩展空间。

预留扩展能力不代表必须在 V1 中实现相应业务功能。

### NFR-008：网络异常处理

**优先级：** Must

通信失败不应被静默地当作业务成功。

系统应能够识别关键通信异常，并根据具体业务采取重试、等待或报告失败等处理方式。

对于执行中的任务，V1 应支持在通信中断时继续执行当前任务，并在恢复连接后补报结果。

### NFR-009：配置与敏感信息管理

**优先级：** Must

运行环境相关的 IP、端口、账号和其他配置应与业务代码分离。

真实密码、访问令牌和其他敏感信息不得直接提交到公开 GitHub 仓库。

### NFR-010：任务状态一致性

**优先级：** Must

任务注入调用方、Edge 和 AMR 应能够通过 `task_id` 关联同一个任务。

系统应区分消息发送、消息接收、任务接受、任务执行和任务完成等不同阶段。

重复消息不得导致重复执行任务、错误覆盖任务终态或重复创建任务。

通信恢复后的状态同步应尽可能收敛到能够解释的最终任务状态。发生无法自动判定的状态冲突时，应记录异常并提供必要的诊断信息。

### NFR-011：任务取消的可控性

**优先级：** Must

任务取消应通过明确的控制接口执行。

系统应区分取消请求、取消处理中和取消完成等阶段，避免将尚未完成的取消操作错误地报告为成功。

取消流程不得绕过机器人自身必要的安全和故障处理机制。

### NFR-012：仿真可复现性

**优先级：** Must

V1 使用仿真代替实际硬件，仿真环境和启动参数应可重复执行。

至少应满足：

* 相同仓库、地图和启动配置可以重复启动仿真环境。
* 仿真 AMR 的 `robot_id`、`instance_id`、命名空间和 Gazebo 模型名称具有明确规则。
* 地图包、世界文件、控制器参数和启动脚本纳入版本管理。
* 关键验证能够通过固定场景和固定配置复现，避免依赖手工临时修改。

### NFR-013：仿真安全边界

**优先级：** Must

在仿真环境中，安全需求仍应以任务状态、导航取消、进程生命周期和异常处理的可验证性为核心。

系统不得以“仿真环境不会造成真实伤害”为由省略错误处理、状态校验和日志记录。任何后续面向真实机器人的迁移都必须重新进行安全评估。

---

## 6. 核心业务流程

### 6.1 AMR 启动与初始化流程

1. Simulation Manager 或其他启动机制启动 AMR。
2. AMR 启动必要的 ROS2 节点。
3. `amr_agent` 通过 HTTP 请求 Bootstrap 配置。
4. AMR 获取仓库标识及 Edge-side MQTT 连接配置。
5. AMR 连接 MQTT Broker 并完成注册。
6. AMR 检查本地地图版本。
7. 必要时下载并校验地图包。
8. 导航模块完成地图加载、定位初始化和就绪检查。
9. AMR 向 Edge Server 上报可用状态。

### 6.2 正常运输任务流程

1. 通过 Edge 侧任务注入入口创建或导入运输任务。
2. Edge Server 校验并保存任务。
3. Edge Server 将任务下发给 `robot_ids` 中的指定 AMR。
4. `amr_agent` 接收并校验任务。
5. `amr_behavior_manager` 接管任务执行流程。
6. `amr_behavior_manager` 调用 `amr_navigation` 执行导航。
7. 导航模块返回执行结果。
8. `amr_behavior_manager` 更新任务状态。
9. `amr_agent` 将任务状态上报 Edge Server。
10. Edge Server 保存最新任务状态。
11. 调用方通过 Edge Server 获取任务执行结果。

### 6.3 任务取消流程

1. Edge 侧任务注入入口或其他上层调用方发起取消请求。
2. Edge Server 校验任务状态并将取消请求转发至参与任务的 AMR。
3. AMR 检查任务标识及任务当前状态。
4. `amr_behavior_manager` 协调任务取消。
5. 必要时调用导航接口取消当前导航动作。
6. AMR 更新任务状态并上报取消结果。
7. Edge Server 保存取消结果。
8. 调用方获取取消后的任务状态。

如果取消请求到达时任务已经进入终态，系统应根据既定规则返回结果，不应错误地覆盖已有的终态记录。

### 6.4 通信中断与结果补报流程

1. AMR 检测到与 Edge Server 的 MQTT 通信中断。
2. AMR 保留当前任务的必要执行状态。
3. 在安全条件允许的情况下，AMR 继续执行当前任务。
4. 任务执行过程中产生的关键状态变化及最终结果被记录为待同步数据。
5. AMR 检测到 MQTT 通信恢复。
6. AMR 重新建立必要的订阅和通信状态。
7. AMR 补报尚未成功送达的任务状态及结果。
8. Edge Server 依据任务标识处理补报消息，并更新最新有效状态。
9. 调用方通过 Edge Server 获取最终任务结果。

### 6.5 仿真状态同步流程

1. Simulation Manager 启动或发现仿真 AMR 实例。
2. Simulation Manager 获取必要的实例信息和运行状态。
3. Simulation Manager 通过 WebSocket 将相关状态同步至 Edge Server。
4. Edge Server 更新仿真 AMR 的相关记录。
5. 当实例状态变化或实例退出时，Simulation Manager 同步相应变化。

### 6.6 关键数据对象

V1 至少应维护以下逻辑数据对象：

| 数据对象 | 关键字段 | 说明 |
| --- | --- | --- |
| 机器人注册信息 | `robot_id`、`instance_id`、`warehouse_id`、`register_timestamp` | Edge Server 维护 AMR 接入信息，区分业务身份与仿真实例。 |
| 机器人运行状态 | `robot_id`、`state`、`online`、`battery`、`pose`、`task_id`、`timestamp` | 用于机器人列表、状态查询和异常识别。 |
| 地图包 | `warehouse_id`、`version`、`package_name`、`package_path`、`package_size`、`upload_time`、`activate` | 记录地图版本、文件位置和激活状态。 |
| 运输任务 | `task_id`、`robot_ids`、`task_type`、`goal`、`status`、`created_at`、`updated_at` | 跨任务注入调用方、Edge、AMR 关联任务，是状态同步和幂等处理的核心。 |
| 任务状态事件 | `task_id`、`status`、`reason`、`timestamp`、`source` | 记录任务状态变化，用于补报、审计和冲突诊断。 |
| 仿真 AMR 实例 | `robot_id`、`instance_id`、`model_name`、`lifecycle_state` | Simulation Manager 与 Edge Server 之间同步的仿真对象。 |

具体数据库表结构、字段类型和索引设计由数据库/接口设计文档定义。字段在实现中可拆分为更细的表，但必须保持上述业务关联能力。

数据库表结构、索引和幂等键见 `docs/architecture/edge-data-model.md`；任务级与 AMR 行为状态机见 `docs/architecture/task-state-machine.md`。

### 6.7 V2 交通路权功能流程（补充）

> V1 的 6.2 正常运输流程整体不变；本节只描述 V2 在「导航执行途中」额外插入的路权闸门。所有场景中 `task_state` 全程不变。

**场景 A：提前授权（正常路径，绝大多数情况）**

1. `amr_behavior_manager` 已发出 `NavigateToPose(D)`，由 Nav2 动态选路；`TrafficGate` **不订阅 `plan`**。
2. `TrafficGate` 消费 `amr_navigate_to_pose` action feedback 的 `current_pose`，中心点 `point_in_poly(center, polygon)` 出现 `false→true` **进区边沿**（拉长的 `L_pull` 已自带提前量，无 `D_trigger` 预判）→ 经 `traffic_request` → `amr_agent` → MQTT 发申请（带 `op=REQUEST` / `ETA` / `lease_ttl_sec`）。
3. Edge 按 FIFO 授权，单播 `response` 经 `amr_agent` → `traffic_response` 回到 `TrafficGate`。
4. `REQUESTING` 收 `GRANTED` **零动作**：不减速、不取消、不重发 goal；车沿原 `NavigateToPose(D)` 直接通过（AC-020）。
5. `GRANTED → HELD` 写入 lease，`lease_expires_at` 开始计时。

**场景 B：被拒等待与恢复（异常路径）**

1. Edge 返回 `REJECTED` / `PENDING`。
2. `TrafficGate` 进程内调 `NavigationClient::cancelNavigation()`，**等待 result 回调把 `m_goal_active` 置回 false**。
3. 按边沿时刻锁存的 `entry_side`，在 `wait_poses[entry_side]` **组内就近**（`argmin dist`）选等待点并 `navigateToPose(wait_pose)`；等待点位于入口端边外侧（上游）。停稳进入 `WAITING`：Nav2 无活动 goal、`cmd_vel` 归零。
4. 每固定 10 s 用同一 `request_id` 幂等重发；**只有 `WAITING` 收到 `GRANTED` 才立即**（不等轮询）重发 `NavigateToPose(D)` → `RESUMING → HELD`。

**场景 C：正常释放**

1. `TrafficGate` 消费 `amr_navigate_to_pose` 的 feedback `current_pose`，中心点 `inside` 出现 `true→false` **出区边沿**（经滞回双阈值去抖）。
2. 立即发 `op=RELEASE`；Edge `HELD → RELEASED`；车不停、不改 goal。掉头从入口退出同样由出区边沿触发，无需单独分支。

**场景 D：区内改道**

1. 授权后车在区内改选绕行路并**驶出通行控制区** → 由「出区边沿」释放（不设独立改道检测）。
2. 车停住区内 / 位姿断流 → 由 `lease_ttl` 到期 `EXPIRED` 回收；不订阅 `plan`、不做 `pathIntersects*` / 改道去抖。
3. 「改道即释放」的**语义保留、独立机制已移除**：由出区 + TTL 承接，仍作为 AC 验收项。

**场景 E：兜底**

1. 导航 `SUCCEEDED` / 任务结束 / 异常 `ABORTED` → 兜底释放（出区边沿漏检时）。
2. `lease_expires_at` 到期 → Edge `EXPIRED` 自动回收（AMR 崩溃 / `cancel` 丢包 / 通信中断 / 区内停住）。
3. AMR 显式 `op=CANCEL`：`PENDING` 出队；`GRANTED` / `HELD` 释放 lease + 配额回退；带旧 `epoch` 的消息一律丢弃。
4. 四触发（出区边沿 / 任务结束 / TTL / 显式 `CANCEL`）取最早释放。

逐跳接口与调用顺序见 `docs/architecture/roadmap/v2-traffic-control-chain.md` §4 / §6，实现级伪代码见 `v2-implementation-spec.md` §5；V0.20 权威口径见 `.workbuddy/artifacts/traffic-gate-8-decisions-frozen.md` §4。

---

## 7. 验收标准

V1 只有在核心需求得到实际验证后，才能视为完成。

| 验收 ID  | 验收内容          | 验收条件                                           |
| ------ | ------------- | ---------------------------------------------- |
| AC-001 | 开发环境构建        | ROS2 工作空间及 Edge Server 均能按文档完成构建               |
| AC-002 | AMR Bootstrap | AMR 能通过 HTTP 获取必要的接入配置                         |
| AC-003 | AMR 注册        | AMR 能通过 Edge-side MQTT 完成注册，Edge 能识别机器人身份      |
| AC-004 | 地图管理          | Edge 能够管理地图包，并识别当前生效地图                         |
| AC-005 | 地图同步          | AMR 能检查地图版本，并在必要时下载地图包                         |
| AC-006 | 导航初始化         | AMR 能完成导航初始化并报告就绪状态                            |
| AC-007 | Edge 任务注入    | 通过 Edge 侧任务注入入口创建任务后，Edge 能接收并识别对应的任务记录                 |
| AC-008 | Edge 任务下发     | Edge 能向 `robot_ids` 中的 AMR 下发任务，AMR 能正确接受任务                 |
| AC-009 | 正常任务执行        | AMR 能通过 `amr_behavior_manager` 调用 Nav2 前往目标位置                    |
| AC-010 | 任务结果反馈        | AMR 能将任务成功或失败状态反馈给 Edge，调用方能获取结果            |
| AC-011 | 任务取消          | 对运行中的任务发起取消请求后，AMR 能执行取消流程并反馈最终结果              |
| AC-012 | 通信中断续行        | 执行中的任务在 MQTT 中断期间，能够在安全条件满足时继续执行               |
| AC-013 | 结果补报          | MQTT 恢复后，AMR 能补报待同步结果，Edge 能正确更新任务状态           |
| AC-014 | 重复消息处理        | 重复的任务或状态消息不会导致重复执行任务或错误覆盖终态                    |
| AC-015 | 仿真同步          | Simulation Manager 能通过 WebSocket 与 Edge 同步仿真状态 |
| AC-016 | 多 AMR 仿真      | 至少两个 AMR 实例能够区分身份并运行于同一仿真环境                    |
| AC-017 | 故障诊断          | 关键初始化、通信、导航和任务失败能够产生可识别的日志                     |
| AC-018 | 文档完整性         | 项目具备基本需求、架构、构建及验证文档                            |
| AC-019 | V2 路权互斥        | 两车同时申请同一窄道，只有一个获批，另一个进入等待；FIFO 排队不串台 |
| AC-020 | V2 导航连续性      | 授权提前到达时零 goal 重发、零停顿；`REQUESTING` 收 `GRANTED` 零动作、只有 `WAITING` 获批才立即重发 D；`NavigateToPose` 目标全程为 D，`task_state` 不变 |
| AC-021 | V2 动态选路        | 走绕行路（从不进区）时零申请、零停顿；区内改道后驶出控制区由出区边沿释放、停住则由 `lease_ttl` 回收；全流程不订阅 `plan` |
| AC-022 | V2 释放            | 中心点 `inside` 出区边沿（`true→false`）即释放；任务结束 / TTL / 显式 `CANCEL` 兜底不漏释放，四触发取最早；释放与导航 Action 结束解耦 |
| AC-023 | V2 取消与时限      | 显式 `cancel` 被正确区分「排队中出队 / 已授权后释放」；`lease_ttl` 到期自动回收；旧 `epoch` 迟到消息被丢弃 |
| AC-024 | V2 电量            | SOC 按 `PowerMode` 档位积分、低电量触发充电、`battery` 不再硬编码；Edge 只读镜像不修改 SOC |
| AC-025 | V2 行为树与急停    | 注入导航失败后 Groot 可见 `Retry` / `Fallback`；软件急停能压住 `cmd_vel` 且可恢复 |
| AC-026 | V2 中心点进出判据与抗抖 | 主判据为 AMR 中心点 `point_in_poly(center, polygon)` 的 `inside` 边沿（进区 `false→true` 发 `REQUEST`、出区 `true→false` 发 `RELEASE`），数据源为 `amr_navigate_to_pose` feedback 的 `current_pose`，1 Hz `robot_pose` 不作主判据；滞回双阈值（内缩进入 / 外扩离开）下边界抖动不产生重复申请或释放；掉头从入口退出触发出区；`crossed()` 已废弃为主判据，仅作侧边异常辅助 |
| AC-027 | V2 进入侧锁存与无 `plan` 订阅 | `entry_side` 由 `s=dot(enter_pose-centroid, long_axis)` 于进区边沿算一次并在本次穿越周期锁存（不每帧重算），等待点取同侧组内 `argmin dist` 且位于入口端边外侧；全流程不订阅 Nav2 `plan`、无 `onPlan()` / `D_trigger` / 改道去抖，提前量由 `L_pull` 提供 |

> AC-001 至 AC-018 属 V1；AC-019 至 AC-027 属 V2，不纳入 V1 完成定义。
>
> AC-026 / AC-027 为 V0.20 顺延新增，状态 `Planned`（设计已定案、代码未实现），已同步 7.1 追溯矩阵与 7.2 实现状态矩阵。

### 7.1 需求与验收追溯矩阵

| 需求编号 | 需求内容 | 主要验收项 |
| --- | --- | --- |
| FR-001 | AMR Bootstrap | AC-002 |
| FR-002 | AMR 注册 | AC-003 |
| FR-003 | 通信重连与状态恢复 | AC-012、AC-013 |
| FR-004 | 地图包管理 | AC-004 |
| FR-005 | AMR 地图版本检查与同步 | AC-005 |
| FR-006 | 导航初始化 | AC-006 |
| FR-007 | Edge 侧任务注入（V1 替代 Cloud 创建） | AC-007 |
| FR-008 | Edge 任务接收与下发 | AC-007、AC-008 |
| FR-009 | 任务接收与执行编排 | AC-008、AC-009 |
| FR-010 | 导航目标执行 | AC-009 |
| FR-011 | 任务状态管理 | AC-010、AC-014 |
| FR-012 | 任务取消 | AC-011 |
| FR-013 | 通信中断期间继续执行当前任务 | AC-012 |
| FR-014 | 通信恢复后的任务状态补报 | AC-013、AC-014 |
| FR-015 | AMR 状态上报 | AC-003、AC-010 |
| FR-016 | 仿真 AMR 生命周期管理 | AC-015、AC-016 |
| FR-017 | 仿真状态同步 | AC-015 |
| FR-018 | 多 AMR 仿真 | AC-016 |
| FR-019 | Cloud 与 Edge 通信 | 后续版本，不纳入 V1 验收 |
| FR-020 | 端到端业务闭环 | AC-007 至 AC-014 |
| FR-021 | 中心点进区边沿触发的路权申请与导航连续性（无 `plan` 订阅） | AC-020、AC-021、AC-026、AC-027 |
| FR-022 | Edge 侧中心仲裁与 lease 生命周期 | AC-019、AC-023 |
| FR-023 | 路权释放与导航结果解耦（中心点出区边沿 + 四触发） | AC-021、AC-022、AC-026 |
| FR-024 | 独立 `traffic_gates.yaml` 几何数据模型与模块归属 | AC-019、AC-020、AC-026、AC-027 |
| FR-025 | 电量消耗与自动充电模拟 | AC-024 |
| FR-026 | 行为树编排与软件急停 | AC-025 |
| NFR-001 至 NFR-013 | 模块化、通信隔离、可构建、可测试、可诊断、可维护、可扩展、网络异常、配置管理、状态一致性、取消可控、仿真可复现、仿真安全 | AC-001、AC-014、AC-016、AC-017、AC-018 |

### 7.2 需求实现状态矩阵

状态定义：

- `Implemented`：代码已实现并可验证。
- `Partial`：已有骨架或部分实现，未满足全部需求。
- `Planned`：已确认设计，尚未实现。

| 需求 | 当前状态 | 说明 |
| --- | --- | --- |
| FR-001 AMR Bootstrap | Implemented | Bootstrap HTTP 返回仓库与 MQTT 接入配置。 |
| FR-002 AMR 注册 | Implemented | 注册请求/响应、RobotManager、初始位姿分配已实现。 |
| FR-003 通信重连与状态恢复 | Partial | MQTT 自动重连、断连置离线、待补报重传已实现，仍需按 Edge/Broker 两种断网方式验证。 |
| FR-004 地图包管理 | Implemented | 上传、下载、查询、激活接口已实现。 |
| FR-005 AMR 地图同步 | Partial | 下载、解压及关键文件存在性校验已实现；无 checksum/size 校验。 |
| FR-006 导航初始化 | Implemented | `navigation_server` 自动拉起 Nav2 并完成 lifecycle/初始位姿就绪。 |
| FR-007 Edge 侧任务注入 | Implemented | HTTP API 与 TaskService 创建、查询、取消已实现。 |
| FR-008 Edge 任务下发 | Implemented | 按 `robot_ids` 向 `amr/{robot_id}/task` 下发任务。 |
| FR-009 任务接收与执行编排 | Partial | 任务状态机、货物申请、单点导航已接通；同一趟暂按单类货物执行。 |
| FR-010 导航目标执行 | Implemented | `amr_navigate_to_pose` Action 全链路已通。 |
| FR-011 任务状态管理 | Implemented | `PENDING→ACCEPTED→RUNNING→SUCCEEDED` 及中断状态持久化已实现。 |
| FR-012 任务取消 | Partial | 取消指令、导航取消、`CANCELLED` 上报已实现，仍需完整回归。 |
| FR-013 通信中断续行 | Partial | Edge 离线检测与当前获批搬运完成逻辑已实现，仍需场景验证。 |
| FR-014 结果补报 | Implemented | `pending_task_reports.json` 原子写入、重传、ACK 清理已实现。 |
| FR-015 AMR 状态上报 | Implemented | 心跳、位姿、聚合状态及当前 `task_id` 已上报。 |
| FR-016 仿真 AMR 生命周期 | Partial | Simulation Manager 生命周期与进程管理已有，待完整回归。 |
| FR-017 仿真状态同步 | Partial | WebSocket `robot_list` 同步已有，待完整回归。 |
| FR-018 多 AMR 仿真 | Partial | 命名空间与身份隔离已有，双机联测待完成。 |
| FR-019 Cloud 与 Edge 通信 | Planned | V2 规划，V1 不交付。 |
| FR-020 端到端业务闭环 | Partial | 正常运输闭环已通，取消与断网恢复闭环待完整验证。 |
| FR-021 按需触发路权与导航连续性 | Planned | 设计已按 V0.20 改写（D-060 / INV-13）：中心点进区边沿触发、`L_pull` 自带提前量、无 `plan` 订阅；代码未实现，4 个导航缺陷为前置阻塞。 |
| FR-022 Edge 中心仲裁与 lease | Planned | 设计已冻结（D-062）；`tbl_resource_lease` 表结构仍属 TBD-020。 |
| FR-023 路权释放与导航解耦 | Planned | 设计已按 V0.20 改写（D-061）：中心点出区边沿 + 四触发取最早（出区 / 任务结束 / TTL / 显式 `CANCEL`）；改道语义由出区 + TTL 承接，不设独立机制。 |
| FR-024 窄道几何与模块归属 | Planned | V0.20 改为独立 `traffic_gates.yaml`（4 点 `polygon` + `wait_poses{l1,l2}`）+ `traffic_gates_editor` 两阶段 + validator 7 条内容校验 + `MapInfo.traffic_gates_yaml` 下发；模块归属仍按 D-057 / D-058 / INV-14；`L_pull` 属 TBD-021、滞回去抖阈值属 TBD-038、loader 读取属 TBD-039。 |
| FR-025 电量消耗与自动充电 | Planned | 设计已冻结（D-064 / TBD-035=B / TBD-036=D）；速率数值属 TBD-022。 |
| FR-026 行为树编排与软件急停 | Planned | C 方案已定；BT 选型属 TBD-023、急停落点属 TBD-024。 |
| NFR-001 模块化 | Partial | 模块边界已有，但部分实现未完成。 |
| NFR-002 通信隔离 | Partial | 通信边界清晰，Cloud MQTT 未实现。 |
| NFR-003 可构建性 | Partial | 当前工作区可构建，但未做干净环境验证。 |
| NFR-004 可测试性 | Planned | 缺少系统化测试。 |
| NFR-005 故障可诊断性 | Partial | 已有日志，缺少统一字段。 |
| NFR-006 可维护性 | Partial | 结构已有，需按模块设计继续完善。 |
| NFR-007 可扩展性 | Partial | 架构预留了交通/电量扩展点。 |
| NFR-008 网络异常处理 | Planned | 断网补报策略已定义，未实现。 |
| NFR-009 配置与敏感信息 | Partial | 配置分离已有，真实密码需清理。 |
| NFR-010 任务状态一致性 | Planned | 幂等规则已定义，未实现。 |
| NFR-011 取消可控性 | Planned | 规则已定义，未实现。 |
| NFR-012 仿真可复现性 | Partial | 世界、地图和启动脚本已有，需固化步骤。 |
| NFR-013 仿真安全边界 | Partial | 文档已明确，需在异常处理中体现。 |

验收过程中应保留必要的构建输出、测试结果、运行日志或截图，便于后续复现与展示。

---

## 8. 后续版本规划

以下内容暂定为 V2 或更后续版本的规划，不属于 V1 的完成条件。

### 8.1 多机器人交通管理（**V2 已正式立项**）

> 本节原为「后续版本规划」。2026-11-20 第七轮项目所有者授权把 V2 需求写入 SRS，本主题已转为 **4.10 节 FR-021 ~ FR-024** 与 **6.7 节** 的正式需求，不再只是规划。

* 路权逻辑**并入 `amr_behavior_manager`（`TrafficGate`），不设独立模块**。
* 实现机器人间的路权请求、Edge 中心 FIFO 仲裁、显式取消与 `lease_ttl` 超时回收。
* 支持中心点进区边沿申请、提前授权零停顿、被拒后进入侧组内就近等待与恢复、中心点出区边沿释放；「改道即释放」语义由出区 + TTL 承接，**不订阅 `plan`**。
* 保留 Nav2 动态选路，不写死路线、不预求全序列。
* V1 不实现交通路权协调；V1 多 AMR 仿真只用于验证身份隔离、基本启动和导航能力。

仍在 `version-roadmap.md` TBD-020 / 021 / 038 / 039 中的数值与字段细节未冻结，按 D-066 处理。**V0.20 口径**：TBD-021 现对应 `L_pull` 拉长量、TBD-038 对应滞回双阈值与去抖窗、TBD-039 对应 `traffic_gates.yaml` loader 读取，均须 V1.5 实测后再定；编号集合不变。

### 8.2 电量管理

* 实现 `amr_power_manager`。
* 管理电池配置及电量状态。
* 实现电量变化模拟。
* 支持充电点、充电状态及自动充电任务。

> **V2 已正式立项**：本主题的设计已转为 **4.11 节 FR-025**（B + D 方案）；速率数值仍属 TBD-022。

### 8.3 高级任务调度

* 支持多个任务的排队和分配。
* 根据机器人状态选择任务执行对象。
* 考虑任务优先级、机器人可用性和任务执行结果。
* 根据实际需求逐步引入更复杂的调度策略。

### 8.4 管理界面与数据平台

* Web 管理界面。
* 仓库、机器人、地图及任务可视化。
* 历史任务查询及统计分析。
* 根据业务规模引入 MySQL、Redis 等数据组件。

上述规划可随项目实际进展调整。进入开发前，应先明确需求、验收条件和对现有架构的影响。

---

## 9. 待确认事项（TBD）

以下问题尚未完全确定，不视为已经冻结的设计决策。

| 编号      | 问题                 | 需要明确的内容                                                |
| ------- | ------------------ | ------------------------------------------------------ |
| TBD-001 | Edge 任务注入接口细节     | 已决策使用 HTTP API、无鉴权、局域网可调用；仍需确认具体错误码枚举和字段校验规则。 |
| TBD-002 | 不可用 AMR 处理          | 已决策：部分 AMR 不可用时任务继续；全部 AMR 异常时 Edge 上报任务级 `INTERRUPTED`。仍需确认异常判定阈值。 |
| TBD-003 | 地图切换策略             | 下载新地图后，何时允许切换，如何保证运行中的任务不受影响？                          |
| TBD-004 | 任务取消的安全策略          | 取消导航时如何处理机器人当前运动，以及如何确认机器人已经达到可接受的停止状态？                |
| TBD-005 | AMR 身份模型           | `robot_id` 与 `instance_id` 的职责，以及真实机器人和仿真实例之间的对应关系是什么？ |
| TBD-006 | MQTT 安全            | V1 使用何种最小可行的身份认证与访问控制策略？                               |
| TBD-007 | 货物点位映射表           | 每个 `goods_id` 的装货区、卸载区、存储区坐标或站点如何维护和加载？                             |
| TBD-008 | MQTT 消息可靠性         | 已决策 QoS 1，并增加 `amr/{robot_id}/task/status/ack` 作为 AMR 状态消息确认。 |
| TBD-009 | 断网期间的任务取消          | 如果任务注入调用方发起取消，但 AMR 正在断网，任务是否必须在恢复通信后取消，还是允许在原任务完成后处理？ |
| TBD-010 | 任务状态冲突             | 如果 Edge 的最后已知状态与 AMR 补报状态不一致，应采用什么规则处理？                |
| TBD-011 | 结果持久化              | 已决策：AMR 使用 `pending_task_reports.json` 原子写入，收到 Edge ACK 后删除对应记录；Edge 使用 SQLite。 |
| TBD-012 | 任务终态后的处理           | 已完成或已取消的任务是否允许重新下发？如允许，应如何区分重试与新任务？                    |
| TBD-013 | 仿真与业务状态关联          | 仿真 AMR 的生命周期状态和 AMR 业务状态如何关联？                          |
| TBD-014 | AMR 任务状态机细节       | 状态机已冻结；心跳 5 秒，超时 5 分钟。 |
| TBD-015 | 地图包内部结构            | V1 地图包是否固定包含 `map.yaml`、`map.pgm`、`zones.yaml`、`stations.yaml`，是否允许其他布局？ |
| TBD-016 | 地图下载地址              | AMR 从注册响应获得的是相对路径还是完整 URL，Edge 如何生成可访问的下载地址？                |
| TBD-017 | Cloud Platform 后续形态  | 已决策 V1 不实现 Cloud Platform；后续版本采用 CLI、HTTP 脚本还是 FastAPI 服务交付，任务创建参数如何输入？ |
| TBD-018 | 仿真启动与身份分配          | 多 AMR 的 `robot_id`、`instance_id`、Gazebo 模型名和 ROS2 命名空间由谁分配并保持一致？ |
| TBD-019 | Edge 故障恢复细节         | 已决策：安全停止、本地待补报、Edge 恢复合并；恢复后不自动继续，需人工或上层重新创建任务。 |

上述问题应在相关模块开始实现前逐步明确，不要求一次性全部解决。涉及任务取消、断网续行和结果补报的关键问题，应在对应功能实现前确定最低可行规则。

> **编号边界**：本表 `TBD-001` ~ `TBD-019` 为 V1 事项。**V2 的 `TBD-020` ~ `TBD-039` 统一维护在 `docs/architecture/version-roadmap.md` 第 5 节与 V2 分册第 9 节**，不在此重复编号。V2 已定案项见本文件 1.6 节 D-057 ~ D-066 与 INV-14。
>
> **V0.20 编号核对**：TBD / INV / FR / D 的既有编号集合**未增删、未改号**；TBD-021（`L_pull` 拉长量）/ TBD-038（滞回双阈值与去抖窗）/ TBD-039（`traffic_gates.yaml` loader 读取）继续保留并改按 8 项定案对应；AC 仅顺延新增 AC-026 / AC-027，7.1 / 7.2 矩阵已同步。

---

## 10. 版本与变更管理

### 10.1 当前版本基线

V1 已交付；当前进入 **V2 开发前设计冻结阶段**。

只有明确标记为 V1 的 Must 需求才是 V1 必须完成的核心交付项；V1 完成定义见 10.3。

**V2 需求已于 2026-11-20 正式纳入本 SRS**（1.6 节决策、4.10 / 4.11 节 FR-021 ~ FR-026、6.7 节流程、7 章 AC-019 ~ AC-027）。V2 的实现任务书为 `docs/architecture/version-roadmap.md` 与 `docs/architecture/roadmap/` 下各分册，实现级规格为 `v2-implementation-spec.md`。

**V0.20 基线（2026-11-22）**：按项目所有者 8 项拍板同步 V2 窄道路权口径 —— 4 点通行控制区、中心点进出边沿判据、无 `plan` 订阅、独立 `traffic_gates.yaml` 与 `traffic_gates_editor`、`MapInfo.traffic_gates_yaml` 下发、「改道即释放」语义由出区 + TTL 承接；D-060 / D-061 / D-063、FR-021 / FR-023 / FR-024、6.7 五场景、AC-019 ~ AC-025 及矩阵同步改写，新增 AC-026 / AC-027。权威口径见 `.workbuddy/artifacts/traffic-gate-8-decisions-frozen.md`，逐项变更见 10.4。

V2 规划项不应自动转化为 V1 的开发任务；V2 未冻结的 TBD 不得假定实现方式。

### 10.2 需求变更原则

新增需求时，应先判断：

1. 是否属于 V1 的必要功能。
2. 是否影响既有模块职责。
3. 是否改变系统通信边界。
4. 是否增加额外依赖或明显提高验证成本。
5. 是否需要同步更新架构、设计和验收文档。

若新增需求不属于 V1 的必要范围，默认记录为后续版本规划，而不是立即实现。

### 10.3 V1 完成定义

当以下条件全部满足时，可认为 V1 达到阶段性完成：

* 核心 Must 需求已实现。
* 核心验收标准已通过。
* Edge 侧任务注入创建任务、Edge 下发、AMR 执行、任务结果反馈的基本闭环能够稳定复现。
* 任务取消流程能够验证，并能正确反馈取消结果。
* MQTT 中断期间当前任务能够在安全条件允许时继续执行。
* 通信恢复后，任务结果能够补报，Edge 与任务注入调用方能够获取正确的最终状态。
* 构建、配置和运行步骤有文档记录。
* 关键通信边界与模块职责符合架构约束。
* 不存在阻止核心业务闭环运行的已知问题。

达到上述条件后，应先整理项目成果、测试证据和文档，再决定是否启动 V2，而不是继续无边界地向 V1 添加功能。

### 10.4 文档修订记录

| 版本 | 日期 | 主要变更 |
| --- | --- | --- |
| V0.1 | 初始版本 | 建立 SRS 基本框架。 |
| V0.2 | 2026-10-09 | 对齐当前代码基线；补充支撑模块、假设依赖、关键接口、数据对象、仿真可复现性 NFR、需求追溯矩阵和 TBD。 |
| V0.3 | 2026-10-09 | 按正式命名将 `amr_manager` 统一修正为 `amr_behavior_manager`，并同步代码包名、命名空间、节点名和接口注释。 |
| V0.4 | 2026-10-09 | 记录 V1 已确认决策：SRS 权威路径、README 定位、Edge 侧任务注入、`instance_id` 命名、V2 模块边界。 |
| V0.5 | 2026-10-10 | 记录 Edge HTTP 任务注入、任务 MQTT Topic/QoS、消息命名规范、ROS2/MQTT 名称集中管理和 ADR。 |
| V0.6 | 2026-10-10 | 记录多 AMR 任务模型：`robot_ids`、Edge→AMR 任务 Topic、任务级共享状态 Topic 和总量不下发平均分配。 |
| V0.7 | 2026-10-10 | 在任务共享状态中增加 `goods_status`，按货物拆分搬运进度。 |
| V0.8 | 2026-10-10 | 增加 AMR 搬运申请/响应流程；明确 `robot_ids` 不能为空；正在搬运货物的 AMR 列表作为 Edge 内部状态。 |
| V0.9 | 2026-10-10 | 增加 AMR 定时任务状态、完成/放弃上报；明确 Edge 数量校准和全部 AMR 异常时的任务级中断。 |
| V0.10 | 2026-10-10 | 将 AMR 任务状态消息中的单货物字段改为 `goods` 数组，支持一趟搬运多类货物。 |
| V0.11 | 2026-10-10 | 确认 HTTP 无鉴权、心跳 5s/超时 5min、货物申请规则、目的地映射方式和 V1 第一验收闭环。 |
| V0.12 | 2026-10-10 | 确认 Edge 故障恢复策略：安全停止、本地待补报文件、Edge 持久化恢复和幂等合并。 |
| V0.13 | 2026-10-11 | 确认任务状态机、AMR 行为状态机、数据模型细节和 `task/status/ack`。 |
| V0.14 | 2026-10-11 | 增加需求实现状态矩阵；补充 HTTP 错误码和 AMR 待补报文件格式引用。 |
| V0.15 | 2026-10-12 | 确认 AMR 内部 ROS2 接口、新消息类型、货物申请、点位映射和导航失败处理。 |
| V0.16 | 2026-10-12 | 确认 map/zone/station/location 关系、zone.goods_ids 映射和任务不携带 target 字段。 |
| V0.17 | 2026-10-12 | 确认 `goods_ids` 使用列表，V1 先假设所有 zone 均为货物相关区域。 |
| V0.18 | 2026-10-22 | 增加 IDLE 充电区域/点位类型与工具选择录入；Edge 解析地图包分配 IDLE 点；注册响应返回 `initial_pose`。 |
| V0.19 | 2026-11-20 | **项目所有者授权纳入 V2 需求**：新增 1.6 节 V2 决策 D-057~D-066、4.10/4.11 节 FR-021~FR-026、6.7 节路权功能流程、7 章 AC-019~AC-025 与追溯/状态矩阵；3.4.2/3.4.4 节登记 V2 已定名 MQTT / ROS2 接口；8.1/8.2 由「规划」改为「V2 已立项」；明确 V2 路权逻辑并入 `amr_behavior_manager`、不设独立模块，全仓清理已废弃的独立交通模块命名；10.1 节版本基线更新。**未改动任何 V1 需求语义。** |
| V0.20 | 2026-11-22 | **同步项目所有者 V2 窄道路权 8 项拍板定案**（权威记录 `.workbuddy/artifacts/traffic-gate-8-decisions-frozen.md`）：① 几何改为独立 `traffic_gates.yaml` 的 4 点凸 `polygon` 通行控制区（端边外移 `min_release_clearance`、侧边拉长 `L_pull`，点序 `P1→P2`/`P2→P3`/`P3→P4`）+ `wait_poses{l1,l2}`，无 `side`/`forward`/`trigger`/`release`；② 主判据改为 AMR 中心点 `point_in_poly(center, polygon)` 进 / 出区边沿（滞回双阈值、feedback `current_pose`，1 Hz `robot_pose` 禁用），`crossed()` 降为侧边异常辅助；③ 进入侧由 `s=dot(enter_pose-centroid, long_axis)` 定侧、`entry_side` 本次穿越周期锁存、组内 `argmin dist` 就近；④ 每侧可多个等待点，不新建 station / 不复用 `WAIT` / 不需要 `TRAFFIC_WAIT`；⑤ S1（`REQUESTING` 收 `GRANTED` 零动作）/ S2（仅 `WAITING` 获批立即重发 D）区分，cancel 后等 result 回调；⑥ 全部省掉 `plan` 订阅、`onPlan()` / `onPlanChanged()` / `pathIntersects*` / `D_trigger`（转译为 `L_pull`）/ 改道去抖；⑦ 兜底全保留（10 s 重试、Edge FIFO + `PENDING` + `tryGrant()`、`lease_ttl`/`EXPIRED`、`epoch`、显式 `CANCEL`、四触发取最早、AC-019~025），「改道即释放」保留语义、移除独立机制，改由出区 + TTL 承接；⑧ 新增 `traffic_gates_editor` 两阶段（4 点 zone + N 等待点）、`MapInfo.msg` 加 `traffic_gates_yaml` 与打包下发 8 处链路、`validator.py` 新增 7 条内容校验。改写 D-060 / D-061 / D-063、FR-021 / FR-023 / FR-024、6.7 五场景、7 章 AC-021 / AC-022，顺延新增 AC-026 / AC-027（`Planned`）并同步 7.1 / 7.2 矩阵、8.1 / 9 章编号边界 / 10.1；3.4.4 补 `traffic_gates_yaml` 字段说明。**TBD / INV / FR / D 既有编号集合不变，TBD-021/038/039 继续保留；未改动任何 V1 需求语义。** |

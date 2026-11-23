# V2 资源仲裁与行为树 · 开发任务书

> **⚠️ 2026-11-22 V0.20 定案变更**：本版按项目所有者 8 项拍板，改为「**4 点凸 `polygon` 通行控制区 + AMR 中心点 `point_in_poly` 进出边沿判据 + 无 `plan` 订阅**」。旧「有向触发段 / 外移释放线 / `crossed()` 主判据 / `NARROW` / `TRAFFIC_WAIT` / `side`」口径**已废弃**，一律以 `.workbuddy/artifacts/traffic-gate-8-decisions-frozen.md` 为准；下文保留的对比语境均显式标注「已废弃 / 历史」。

**Status:** Planned
**所属路线图:** [`../version-roadmap.md`](../version-roadmap.md)
**版本主题:** 统一资源仲裁（交通路权 / 充电点 / 货物配额）+ 电量真实化 + 行为树执行骨架 + 软件急停
**前置依赖:** V1.5 验证基线完成（故障注入与取证能力可用）

---

## 1. 目标

把 V1 中散落的「申请—批准—占用—释放」逻辑抽象成一套统一资源仲裁机制，同时让交通与电量成为该机制的两种资源实例；引入行为树作为**单趟执行的骨架**，把 V1 散在 if-else 里的恢复逻辑收敛成可组合结构；补齐工业 AMR 必备的软件急停与指令优先级。

> **为什么这三件事放在同一版**：V2 的两个新功能（路权、低电量）本身就是行为树的两个条件节点。若先用 if-else 实现、以后再引入行为树，等于把恢复逻辑写死两次；合并实施既省代码，又能讲清「抽象与分层」的架构判断。

---

## 2. 范围边界

### 2.1 In Scope

- 统一资源仲裁层（新增内部服务/模块，具体形态见 3.1）。
- 交通路权：申请、排队、批准、等待、显式取消、超时回收、死锁避免；**运行时按需触发主模型**（AMR 中心点进区 `false→true` 边沿申请 + 出区 `true→false` 边沿释放 + 被拒后同侧就近等待点重导航 + 目标全程 D；**不订阅 `plan`**），见 3.2.2。
- 地图几何扩展：独立 `traffic_gates.yaml`（`resource_id` + 4 点凸 `polygon` 通行控制区 + `wait_poses{l1,l2}` 每侧多个等待点）；**不录路线顺序、不预求全序列、不做单段前瞻、不扩展 zone/station type**，见 3.2.1。
- 电量真实化：移除硬编码、消耗模型、低电量策略、自动回充、充电点占用管理。
- 行为树执行骨架：BT 依赖、节点、XML、与 `task_state` 的映射、Groot 可视化。
- 软件急停与 `cmd_vel` 优先级仲裁。
- 降级与恢复策略矩阵（文档 + 实现）。
- 与 V1.5 故障注入框架的联动验证。

### 2.2 Out of Scope（严格禁止）

| 不做 | 归属 |
| --- | --- |
| Cloud Platform、Web 界面、MySQL/Redis、权限审计 | V3 / 更后 |
| 机械臂、夹爪、MoveIt、视觉识别、货物物理挂载 | V3 / V4 |
| 高级任务调度（SRS 8.3：任务排队、按机器人状态选机、优先级调度） | V3 或更后 |
| 修改任务级 `state` 状态机、AMR `task_state` 枚举、取消语义 | 冻结（INV-01/02/03） |
| 改变目的地映射方式（下发坐标） | 冻结（INV-06） |
| 改变 `goods_status` 四量不变式、幂等表、ACK、待补报 | 冻结（INV-07/08/09） |
| 大规模重构 `amr_agent` / `navigation_server` 既有逻辑 | 违反项目规则 |

### 2.3 关键设计红线

**任务级 FSM 一个字节不改。** 行为树只负责「单趟怎么执行」，对外 `task_state`、5 秒上报、`status/ack`、待补报、取消语义全部保持 V1 契约（INV-01~03）。实现者若发现需要改对外状态，必须先提出方案并征得同意。

---

## 3. 功能细节

### 3.0 功能 → 节点归属总表（实现前必读）

> **本节回答「V2 每个功能落在哪个节点 / 哪一侧」。** 大原则：**Edge 管共享资源（谁占路口、谁占充电桩），AMR 管本体状态（电量、行为、急停）**。

| 功能 | 承载节点 / 模块 | 所属侧 | 交互对象 | 归属依据 |
| --- | --- | --- | --- | --- |
| 任务级 FSM、5s 状态上报、ACK、待补报 | `amr_agent` + Edge `TaskService` | 两侧 | MQTT `amr/{robot_id}/status` | V1 已实现，冻结 |
| 单趟执行编排、Retry / Fallback / 抢占 | `amr_behavior_manager`（**内嵌 BT**） | AMR · `robot_ws` | `navigation_server`、`amr_power_manager` | 3.4.5：BT 完全位于该包内部 |
| **电量真值 SOC、消耗/充电积分、电池配置、充电状态机** | **`amr_power_manager`（V2 新增）** | **AMR · `robot_ws`** | 向 behavior manager 提供 `BatteryOK`、向 `amr_agent` 提供 battery 数值 | `SRS 8.2`、`system.md:34`、`amr.md:149-160` |
| 三类共享资源仲裁（GOODS_QUOTA / TRAFFIC_RIGHT / CHARGE_POINT） | `ResourceBroker`（V2 新增） | Edge · `edge_server` | SQLite `tbl_resource_lease` + MQTT | 3.1 |
| 路权申请 / 排队 / 显式取消 / 响应处理 / 占用时限回收 | `traffic_rights_service`（V2 新增） | Edge | AMR 侧 BT `RequestOnApproach` / `WaitAndRetry` | 3.1.2 / 3.2.2 / 3.2.5 |
| 充电点分配与占用 | `IdlePointAllocator` 升级为 lease | Edge | SQLite | 3.1.5、D-053/054 |
| battery **只读镜像** | `tbl_robot_status.battery` | Edge | 由 `amr_agent` 5s 上报写入 | 3.3.1；SRS 961 行：用于列表/查询/异常识别 |
| 路权本地按需触发 / 等待 / 恢复 | **`amr_behavior_manager` 内 `TrafficGate`（类 + BT 节点）· 已定案，不设独立模块 / ROS2 包** | AMR | 只读 `amr_navigate_to_pose` action feedback 的 `current_pose`（中心点）+ `traffic_gates.yaml` 的 4 点 `polygon` / `wait_poses`；中心点进出边沿判定，**不订阅 `plan`**；进程内调 `NavigationClient`；经 `traffic_request` / `traffic_response` 与 `amr_agent` 的 `TrafficBridge` 交互 | INV-14（2026-11-20 第七轮）；控制链 §3 / §4；实现规格 §3 |
| 软件急停与 `cmd_vel` 优先级仲裁 | 落点待定（新节点 or `navigation_server` 内） | AMR | `cmd_vel` mux | TBD-024 |
| 电量数据源 | **B + D 方案**：本地速率积分（IDLE/WORK 两档消耗 + 充电），不引入独立充电源 | `amr_power_manager` | AMR · `robot_ws` | 行为层 `PowerMode` 声明档位、`amr_agent` 复用已有 `robot_status` 订阅取数值、Edge 只读镜像；**TBD-035 已冻结为 B、TBD-036 已冻结为 D**，见 3.3.2 / 3.3.2.3 / 3.3.5 |
| 消耗档位判定信号 | 行为层单向发布的 `PowerMode`（专用 msg，不含 `battery`）；**不订阅** `cmd_vel` / `robot_status` / `task_status` / `task_state` | `amr_behavior_manager` 发布、`amr_power_manager` 订阅 | AMR · `robot_ws` | **TBD-036 已冻结为 D**：行为层声明、电量层查表积分（零判定），见 3.3.2.1 / 3.3.2.3 |

#### 3.0.1 归属更正与职责红线

**更正（本任务书上一版的错误）**：第 5 节原将 `power_manager.*` 写为 `edge_server/src/service/` 下的新增服务，与 `SRS 8.2`（「实现 `amr_power_manager`」）、`system.md:34`（`amr_power_manager` 位于 AMR 分组）、`amr.md:149-160`（职责为电池配置、状态监控、电量上报、充电状态管理、向 behavior manager 提供电源状态）**冲突**。现更正为：

- **AMR 侧 `amr_power_manager`（新 ROS2 包）** = 电量真值唯一持有者 + 消耗/充电积分 + 充电状态机 + 电池配置。
- **Edge 侧** = `CHARGE_POINT` 资源仲裁 + battery 只读镜像 + 低电量准入策略（可选）。
- **职责红线**：**Edge 侧不得产生、累加或修改 SOC 数值**。理由：电量是 BMS / 库仑计估出的本体状态；若两侧都能改数值，会重新引入 V1 已解决的「状态收敛」问题，且断网时无法自洽。

**V2 路权模块归属（2026-11-20 第七轮项目所有者定案，INV-14）**：**不设独立模块**。路权逻辑 = `amr_behavior_manager` 包内的 **`TrafficGate`**（一组类 + BT 节点），职责是「决定运动何时等待 / 恢复」，不做全局任务排序、不持有全局 lease、不改 `task_state`。`amr_agent` 只补 `TrafficBridge` 透明桥，**不新建独立进程、不新建 ROS2 包、不新增 MQTT client**。

> **消歧（第六轮提出的打架表述已消除）**：旧版 3.0 归属表的「并入 `amr_agent` 通信层」应理解为「**复用 `amr_agent` 的 MQTT 通道**」，不是把判定逻辑搬进 `amr_agent`；旧版 3.0.1 的「形态待定 / 待 TBD-020 确认」**已删除**。位姿（action feedback 的 `current_pose`）、`traffic_gates.yaml` 的 4 点 `polygon` / `wait_poses`、`NavigationClient::cancelNavigation()` 全在 `amr_behavior_manager` 进程内（**无 `plan` 订阅**），逻辑放 `amr_agent` 会凭空多出三条跨进程链路。完整理由与逐跳接口见 [V2 交通路权控制链 §3 / §4](v2-traffic-control-chain.md)，命名与消息字段见 [V2 实现规格 §1](v2-implementation-spec.md)。
>
> **全仓命名红线**：文档与代码**不得再使用已废弃的独立交通模块命名**（历史与 V1 边界声明已统一改写）。

> **为什么这么定**：共享资源必须中心仲裁（否则两车同时占同一路口/充电桩），本体状态必须本地持有（否则断网即失效）。这条边界与 V1 的「Edge 侧共享状态 / AMR 本地待补报」是同一套思路，不是新范式。

### 3.1 统一资源仲裁层（ResourceBroker）

#### 3.1.1 动机与 V1 地基

V1 已有同类机制，V2 是**泛化而非新建**：

| V1 已有 | 文件证据 | 可复用部分 |
| --- | --- | --- |
| 货物配额原子扣减 | `edge_server/src/database/sqlite_db.cpp::ApproveGoodsAllocation`：`UPDATE ... SET in_progress_quantity = in_progress_quantity + ?, pending_quantity = pending_quantity - ? WHERE ... AND pending_quantity >= ?` | 原子条件更新范式 |
| 配额幂等与结算 | `tbl_task_goods_allocation` 的 `UNIQUE(request_id, robot_id)`、`state IN ('IN_PROGRESS','COMPLETED','ABORTED')`、`SettleGoodsAllocation` | lease 状态流转 |
| 充电点分配 | `edge_server/src/service/idle_point_allocator.cpp`（212 行）：顺序分配 + `m_occupied` 占用标记；SRS D-053/D-054 | 充电点资源化 |
| 路权 Topic | `common/mqtt/mqtt_topic.h` 的 `TRAFFIC_RIGHTS_REQ` / `TRAFFIC_RIGHTS_RSP_PREFIX`（V1 仅字符串常量、无 `robot_id` 层，与通配订阅不自洽）；订阅被注释于 `amr_communication.cpp:111` | **V2 替换为已定名常量**：`AMR_TRAFFIC_REQUEST` / `AMR_TRAFFIC_RESPONSE` / `AMR_TRAFFIC_SUBSCRIBE`，见 INV-14 与实现规格 §1 |
| 幂等事件表 | `tbl_amr_task_status_event` + `INSERT OR IGNORE` | 请求幂等 |

#### 3.1.2 抽象模型（命名与消息已冻结；表结构与数值参数仍属 TBD-020）

三类资源共享同一套语义：

| 概念 | 说明 |
| --- | --- |
| `resource_type` | `GOODS_QUOTA` / `TRAFFIC_RIGHT` / `CHARGE_POINT` |
| `resource_id` | 配额用 `task_id + goods_id`；路权用路口/区域 id；充电点用 `station_id + location_id` |
| `holder` | `robot_id` |
| `request_id` | 与 V1 配额一致的幂等标识，由申请方生成 |
| `state` | `PENDING`（排队中，不占资源）`-> GRANTED -> HELD -> RELEASED`；异常分支 `EXPIRED` / `REVOKED` / `CANCELLED` |
| `lease_expires_at` | 租约到期时间，由**申请时声明的 `lease_ttl`** 在授权时写入，超时自动回收 |
| `queue_seq` / `epoch` | 排队序号（FIFO 依据）与请求代次（丢弃迟到陈旧消息） |
| `granted_at` / `released_at` | 时间戳，用于故障恢复与审计 |

**必须满足的约束**：

1. 授权必须是**原子**的：同一资源在同一时刻只能有一个 holder（条件更新或唯一约束实现）。
2. 请求必须**幂等**：相同 `request_id` 重复申请返回同一结果，不重复占用、不产生第二份 lease。
3. 必须有**超时回收**：申请时声明 `lease_ttl`，授权时写入 `lease_expires_at`；holder 掉线 / 心跳超时（V1 为 5 分钟）/ 租约到期 / 取消消息丢失时自动释放。这是「中心点出区边沿释放」之外的一等兜底，见 3.2.3。
4. **排队必须在 Edge 侧**，按 `queue_seq` FIFO：Edge 是唯一持有全局 lease 视图的一方，AMR 各自看不到对向车，不得在 AMR 侧自行排序或抢占。
5. **取消必须是一等显式事件**：Edge 不得靠「AMR 不再重试 / 断线 / 静默」推断取消；取消消息带 `request_id` + `epoch`，按状态分「排队中出队」与「已授权后释放」两种语义处理，见 3.2.5。
6. Edge 重启后必须能从 SQLite **重建占用状态**，不得出现「资源永久被锁」。
7. 释放必须可回退配额：对应 `SettleGoodsAllocation` 的 `COMPLETED` / `ABORTED` 语义。

#### 3.1.3 建议新增数据表（TBD-020）

```sql
CREATE TABLE IF NOT EXISTS tbl_resource_lease
(
    id                INTEGER PRIMARY KEY AUTOINCREMENT,
    request_id        TEXT NOT NULL,
    resource_type     TEXT NOT NULL
                      CHECK(resource_type IN ('GOODS_QUOTA','TRAFFIC_RIGHT','CHARGE_POINT')),
    resource_id       TEXT NOT NULL,
    holder_robot_id   TEXT NOT NULL,
    state             TEXT NOT NULL DEFAULT 'GRANTED'
                      CHECK(state IN ('PENDING','GRANTED','HELD','RELEASED','EXPIRED','REVOKED','CANCELLED')),
    lease_expires_at  TEXT,
    granted_at        TEXT NOT NULL,
    released_at       TEXT,
    UNIQUE(request_id, resource_type, resource_id),
    UNIQUE(resource_type, resource_id, state)   -- 部分数据库需改为条件唯一索引
);
```

> 说明：最后一条 `UNIQUE` 在 SQLite 中会把 `RELEASED` 也纳入唯一约束，导致同一资源无法二次分配。**实现时应改为部分唯一索引**：
> `CREATE UNIQUE INDEX idx_lease_active ON tbl_resource_lease(resource_type, resource_id) WHERE state IN ('GRANTED','HELD');`
> **`PENDING`（排队中）刻意不进入该索引** —— 排队的请求不占用资源，同一 `resource_id` 可有多个 `PENDING` 共存，由 Edge 按 `queue_seq` 依次授权。此为实现细节，落地时以实际 SQL 验证为准。

**是否复用 `tbl_task_goods_allocation` 而不新建表**：TBD-020 需明确。若复用，需扩展 `resource_type` 字段并评估对 V1 幂等语义的影响；若新建，需明确配额类 lease 与 `tbl_task_goods_allocation` 的双写一致性策略。**不得在未确认前自行选择。**

#### 3.1.4 死锁避免

采用**有序资源申请**：所有需要同时持有多个资源的 AMR 必须按 `resource_id` 字典序依次申请，禁止逆序持有；申请失败则释放已持有资源并退避重试。

> **为什么这么定**：路口 + 充电点组合下最典型的死锁是「A 占路口等充电桩、B 占充电桩等路口」。有序申请从协议层消除循环等待，比实现死锁检测+回滚更简单、更可验证，也是面试里能讲清楚的取舍。
>
> **V2 复杂度边界（项目所有者 2026-11-20 确认）**：只做「单资源单 lease + FIFO 排队 + 显式取消 + TTL 回收 + 幂等 + `epoch` 防陈旧」。**不做**优先级抢占、多资源联合预约、死锁检测与回滚、AMR 侧排队状态广播。中心排队引入的复杂度用边界收敛，而不是靠加机制去覆盖。

#### 3.1.5 Edge 重启恢复

- 启动时扫描 `tbl_resource_lease` 中 `state IN ('GRANTED','HELD')` 且未过期的记录，按 `holder_robot_id` 与 `tbl_robot_status.online` 比对。
- holder 不在线或心跳超时 → 标记 `EXPIRED` 并释放资源。
- holder 在线 → 保留租约并向 AMR 重发授权，避免「车以为自己有路权、Edge 以为没有」。

---

### 3.2 交通权限

#### 3.2.1 区域建模（TBD-021 / TBD-039）

> **本轮定案（2026-11-22 V0.20 八项拍板，覆盖 2026-11-20 第五轮旧口径）**：项目所有者确认**运行时按需触发主模型** —— 不预先求 `[N1..Nn]` 全序列、不做单段前瞻、不录路线顺序；`NavigateToPose` 目标**全程为最终目的地 D**，由 Nav2 动态选路。路权挂在**4 点凸 `polygon` 通行控制区**上，主判据为 **AMR 中心点 `point_in_poly(center, polygon)` 的 `inside` 边沿**：进区 `false→true` 申请、出区 `true→false` 释放，**不是跨线事件、不是 footprint**；`crossed()` 降级为侧边异常识别辅助。数据源仍是 `amr_navigate_to_pose` action feedback 的 `current_pose`（`amr_behavior_manager_node.cpp:125-129` 空回调填实），1 Hz `robot_pose` 禁止作主判据；仍需**边沿检测 + 滞回双阈值**（内缩进入 / 外扩离开，TBD-038）。方向无关 → 掉头从入口退出天然触发「出区」，旧「反向跨 trigger 无退出分支」缺口自动消失。区域端边外移 `min_release_clearance`、侧边沿通道拉长 `L_pull`（自带提前量），**不订阅 `plan`**（原 `onPlan()` / `onPlanChanged()` / `D_trigger` 已废弃）；被拒才 cancel 当前导航并到**同侧就近**等待点等待（进入侧由 `dot(enter_pose-centroid, long_axis)` 符号定 L1/L2，本次穿越周期锁存）。改道即释放**保留语义、移除独立机制**（驶离控制区由出区边沿释放、停住 / 断流由 TTL 承接）。申请时带权限占用时限（`lease_ttl`）作兜底。决策依据见 `.workbuddy/artifacts/traffic-gate-8-decisions-frozen.md`（8 项定案）与 `.workbuddy/artifacts/traffic-gates-editor-and-flow-spec.md` §5（editor 两阶段 / YAML 草案 / validator 7 条）。

##### 3.2.1.1 现状核查（读代码所得，实现者必读）

| 位置 | 现有事实 | 对 V2 的影响 |
| --- | --- | --- |
| `data/warehouse_01/zones.yaml` | 6 个 zone；`type: UNLOAD / LOAD / STORAGE / IDLE`；`goods_ids: []` 或 `[A]` / `[B]`；**已有 `polygon`（4 点，世界坐标，米）** | **保持 V1 原样、不扩展 `type` 取值**（旧「加 `NARROW`」已废弃）；窄道几何改放独立 `traffic_gates.yaml`，4 点 `polygon` 是**主判据 / validator / 可视化的数据**，不再是 zone 的静态校验附属 |
| `data/warehouse_01/stations.yaml` | 6 个 station，每个 2 个 location；`type: WORK / WAIT / IDLE`；`pose: {x, y, yaw}` | **保持 V1 原样**：等待点改放 `traffic_gates.yaml` 的 `wait_poses{l1,l2}`，**不新建 station、不复用 `WAIT`、不需要 `TRAFFIC_WAIT`** |
| `data/warehouse_01/walls.yaml` | 6 条线段，构成通道 | 窄道由墙形成；控制区 `polygon` 需校验不与墙冲突、通道宽度足够 |
| `data/warehouse_01/map/warehouse_01.yaml` | `resolution: 0.05`、`origin: [-6.02, -5.48, 0]` | 几何单位与坐标系基准 |
| `data/warehouse_01/metadata.yaml` | `version: '2.0'`、`files` 映射 zone/station/wall | 新增第 4 文件 `traffic_gates.yaml` 需升版本，引用 TBD-003 |
| `tool/map_tool/validator.py:59-72` | **只检查三个文件是否存在，无任何内容 / 枚举校验** | **新增 7 条内容校验**（4 点凸 / 点序 / 端边 `L_pull` / 与墙不冲突 / 等待点分组且位于端边外侧 / 不与分叉点前通道重叠 / `resource_id` 唯一），原「同步校验」说法已废弃 |
| `warehouse_tool/src/zone_editor_node.cpp:186-208` | zone type 菜单硬编码 4 选 1（UNLOAD/LOAD/STORAGE/IDLE） | **不扩展 `selectZoneType()` 菜单、`NARROW` 已废弃**；新建独立 `traffic_gates_editor`，仅复用 `ClickedPointReceiver` / `ZoneMarkerPublisher` |
| `warehouse_tool/src/station_editor_node.cpp:257-277` | location type 菜单硬编码 3 选 1（WORK/WAIT/IDLE） | **不扩展 location type 菜单、不加 `TRAFFIC_WAIT` / `side`（已废弃）**；等待点由 `traffic_gates_editor` 阶段 B 采集、`yaw` 由长轴派生 |
| `amr_behavior_manager/src/zone_station_loader.cpp:16-31` | `ZoneInfo` **无 `polygon` 字段**，`load()` 只读 `id/name/type/goods_ids` | V1 `ZoneInfo` 无几何属实，但**不扩展它**；新建独立 `traffic_gate_loader` 读 `traffic_gates.yaml` 的 4 点 `polygon` / `wait_poses`（TBD-039），供中心点进出边沿判定 |
| `amr_interfaces/msg/MapInfo.msg` | `zones_yaml` / `stations_yaml` 为 `string`，AMR 侧用 `YAML::LoadFile(...)` | 新增 `traffic_gates_yaml` 字段，打包下发 8 处链路；下发的是**文件路径**，不是内容 |
| `amr_behavior_manager/src/goods_point_resolver.cpp:77-97` | 只找 `type == "WORK"` 的 location，找不到退 `WAIT` | **V2 不新增任何 location 类型**，V1 点位解析零改动（这正是「不新建 station」的理由） |

**关键结论**：V1 数据模型里**没有任何「路线 / 途经点 / 邻接 / 顺序」**，也没有窄道几何。`GoodsPointResolver` 只按 `goods_id + task_type` 经 `map -> zone -> station -> location` 解析出**一个目标点**（INV-06 不下发坐标），路径完全由 Nav2 在栅格地图上规划。因此「A→B→C→D」中的 B、C **不是录入的途经点**，而是「运行时 AMR 中心点进入 4 点凸 `polygon` 通行控制区」这一事实（由 `inside` 边沿判定，与方向无关）。窄道几何放**独立 `traffic_gates.yaml`**，V1 的 `zones.yaml` / `stations.yaml` / `zone_station_loader` / `goods_point_resolver` **一律不动**。旧版「任务开始 `ComputePathToPose(current→D)` 求全序列」已废弃：它会形成第二套路径真值并把 Nav2 的动态选路锁死（项目所有者 2026-11-20 明确纠正）。

##### 3.2.1.2 核心决策：运行时按需触发（zone reservation）

| | 静态（人工 / 工具录入） | 运行时自动（不录入） |
| --- | --- | --- |
| 内容 | 独立 `traffic_gates.yaml`：`resource_id` + 4 点凸 `polygon` 通行控制区（点序 `P1→P2` 端边 L1 / `P2→P3` 长轴 / `P3→P4` 端边 L2；端边外移 `min_release_clearance`、侧边拉长 `L_pull`）+ 每侧 ≥1 个等待点 `wait_poses{l1,l2}`；**无 `side` / `forward` / `trigger` / `release` 字段** | 这一趟是否进 / 出控制区、何时按 `inside` 边沿申请 / 释放、Nav2 走哪条路 |
| 录入方式 | `traffic_gates_editor` 两阶段（阶段 A 点 4 下录 zone + 终端填 `resource_id`；阶段 B 点 N 下录等待点，工具按几何归档到 L1 / L2 外侧两组、`yaw` 由长轴派生） | 只读 `amr_navigate_to_pose` action feedback 的 `current_pose`（中心点）；**不订阅 `plan`**，1 Hz `robot_pose` 禁止作主判据 |
| 反向 A→D / D→A | **同一份数据，不改、不录第二份** | **方向无关**：进 / 出区只看 `inside` 边沿，掉头从入口退出天然触发「出区」；进入侧由 `dot(enter_pose-centroid, long_axis)` 符号定 L1 / L2 并锁存 |
| 地图改动 | 新增独立 `traffic_gates.yaml` + `MapInfo.traffic_gates_yaml` 字段（打包下发 8 处链路）+ 升版本，V1 zones/stations 不动 | 自动跟随 |

**判定机制（不写 `goal`、不改 costmap、不下发途经点；无 `plan` 订阅）**：

```text
进入侧算法（进区边沿时刻执行一次，不用 crossed）：
    long_axis  = normalize(P3 - P2)                    # 点序派生，不落盘
    centroid   = mean(P1..P4)
    s          = dot(enter_pose - centroid, long_axis)
    entry_side = (s < 0) ? L1 : L2                    # 本次穿越周期锁存，不每帧重算
    wait_pose  = argmin dist(enter_pose, wait_poses[entry_side])   # 组内就近

主判据：on feedback->current_pose（AMR 中心点 center）
    inside = point_in_poly(center, polygon)           # 滞回双阈值：进入内缩 d_hyst / 离开外扩 d_hyst
    inside false->true（进区边沿）+ 状态过滤 + 本趟未处理过该 resource_id
        -> 算一次 entry_side / wait_pose；emit TrafficRequest(request_id, robot_id, resource_id, ETA, lease_ttl)；不动 goal
    inside true->false（出区边沿）+ 已持权
        -> emit op=RELEASE（与导航结果解耦）

辅助：crossed(prev, cur, 侧边) 命中侧边异常穿入 -> 按 s 符号兜底定侧 + LOG_WARN（validator 约束侧边不与公共通道重叠）
省掉：plan 订阅 / onPlan() / onPlanChanged() / pathIntersects* / D_trigger / 改道去抖变量
```

- **主判据 = 中心点 `point_in_poly` 的 `inside` 边沿**，不是跨线事件、不是逐帧取车体外轮廓与区域求交的 `footprint` 判定。`crossed()` 降级为「从侧边穿入」的异常识别辅助（配合上面的 `s` 兜底）与 validator 可视化，不再是主判据（红线见 3.2.3）。仍需**边沿检测 + 滞回双阈值**（内缩进入 / 外扩离开，TBD-038）+ 状态过滤：未持权忽略「离开」、已持权（`HELD` / `RESUMING`）忽略「再进入」，防边界抖动重复请求。
- **走绕行路根本不会进入控制区 → 零申请、零停顿**（端边不压分叉点，保 S6）。系统不需要知道有几条路线，也不需要预先决定走哪条；Nav2 保留完整动态选路能力。这是工业 AMR 集群常见的「基于区域的预约」而非「基于路线的锁」。
- 位姿来源 `navigation_manager.cpp:87-114` 已把 `current_pose` 上抛，`amr_behavior_manager_node.cpp:125-129` 的 feedback 空回调 V2 填实；**不订阅 `plan`**（`nav2_view.rviz:302` 的 `plan` 仅 RViz 显示配置，V2 不新增订阅）；`robot_pose`（1 Hz）禁止作主判据，仅作初始位姿 / 兜底。

> **为什么废弃「任务开始求全序列」**：`ComputePathToPose(current→D)` 得到的是**某个 costmap 快照下的路径**，与 `bt_navigator` 内部后续规划是不同时刻、不同 planner 实例的两份产物；把它当成路权顺序，等于引入第二套路径真值，且人工 / 预录顺序会锁死 Nav2 的改道。连带删除：全序列求交、弧长排序、序列复核、单段前瞻、`IsPathValid` / 路径缓存（见 3.2.2）。

##### 3.2.1.3 数据模型（独立 `traffic_gates.yaml`，字段命名以 TBD-021 冻结为准）

**不复用 V1 层级**：窄道数据放**独立 `traffic_gates.yaml`**，`MapInfo.msg` 加 `traffic_gates_yaml` 字段随地图包下发（打包 / 下发 8 处链路）。V1 的 `zones.yaml` / `stations.yaml` / `ZoneInfo` / `Location` / `zone_station_loader` / `goods_point_resolver` **一律不动**；**不新建 station、不复用 `WAIT`、不需要 `TRAFFIC_WAIT`、不扩展 zone/station type 菜单**。工具侧用独立 `traffic_gates_editor`（两阶段）采集，见 3.2.1.4。

```yaml
# traffic_gates.yaml —— 独立文件，字段命名 / L_pull 数值由 TBD-021 冻结
traffic_gates:
  - resource_id: crossing_01
    polygon:                        # 4 点凸通行控制区；点序冻结：P1→P2 端边 L1 / P2→P3 长轴 / P3→P4 端边 L2
      - [x1, y1]                    # P1
      - [x2, y2]                    # P2
      - [x3, y3]                    # P3
      - [x4, y4]                    # P4
    wait_poses:
      l1:                           # L1 端边外侧（上游），每侧 ≥ 1 个；点在区域内由工具拒绝重录
        - {x: .., y: .., yaw: ..}
        - {x: .., y: .., yaw: ..}
      l2:                           # L2 端边外侧（上游），每侧 ≥ 1 个
        - {x: .., y: .., yaw: ..}
```

字段说明：`yaw` 由工具按长轴 `P2→P3` 派生（不让人工填）；**不设 `side` / `forward` / `trigger` / `release` 字段** —— 进入侧 L1 / L2 由运行时 `s` 符号决定，等待点归哪一组由 editor 阶段 B 按几何归档，方向无关、正反一份数据。

**中心点判据（唯一主判据）**：`inside = point_in_poly(center, polygon)`，取 `amr_navigate_to_pose` action feedback 的 `current_pose`（`amr_behavior_manager_node.cpp:125-129` 空回调填实）；1 Hz `robot_pose` 禁止作主判据。进区 `false→true` 发 `op=REQUEST`（不动 goal）、出区 `true→false` 发 `op=RELEASE`；仍需**边沿检测 + 滞回双阈值**（进入用内缩 `d_hyst`、离用外扩 `d_hyst`，TBD-038）+ 状态过滤。`crossed()` 仅作侧边异常识别辅助；`zonePolygon()` 保留（validator / 异常裁决 / 可视化）。

**进入侧算法（不用 `crossed()`）**：`long_axis = normalize(P3-P2)`、`centroid = mean(P1..P4)`、边沿时刻 `s = dot(enter_pose - centroid, long_axis)`；`s<0 → L1 / wait_poses.l1`、`s>0 → L2 / wait_poses.l2`；组内 `argmin dist(enter_pose, p)`。**`entry_side` 在本次穿越周期锁存，不能每帧重算**（否则车到区域中部 `s≈0` 会翻侧、选到对侧等待点）；侧边异常穿入按 `s` 兜底 + `LOG_WARN`。

**几何 = 4 点凸 `polygon` 的通行控制区，不是 3×2 物理窄道**：两条端边（L1 = `P1→P2`、L2 = `P3→P4`）外移 `min_release_clearance` 到宽敞处（中心过端边 ⇒ 车尾已出窄道），两条侧边沿通道方向拉长 `L_pull` 把提前量藏进「进区 → 到真实窄道口」的行驶时间（`D_trigger` 转译为 `L_pull`）；点序 `P1→P2` 端边 L1 / `P2→P3` 长轴 / `P3→P4` 端边 L2。

**三条几何硬约束**：

1. **等待点必须在入口端边外侧（上游）**、每侧 ≥ 1 个，等待路径不得穿过真实窄道；否则「申请未过 → 导航到等待点」会让车无权限穿越窄道。由 editor 阶段 B 几何归档 + validator 静态校验保证。
2. **端边外移 `min_release_clearance`**、**侧边拉长 `L_pull`**，`L_pull ≥ v_max × (RTT_拒绝_p99 + T_cancel + T_newgoal_accept) + v_max²/(2·abs(a_dec)) + d_safety`。用 `nav2_params.yaml:146` `max_vel_x=0.26`、`:157` `decel_lim_x=-0.5` 估算约 0.8~1.0 m 量级，数值由 V1.5 基准实验实测并入 TBD-021 / 038；未实测前不得假设 0.8 m 够用。`xy_goal_tolerance` / `yaw_goal_tolerance` 不参与进出判定。
3. **端边不压分叉点**（区域不与分叉点之前的公共通道重叠）：走绕行路的车根本不进区 → 零申请、零停顿（S6「走绕行路零申请」）；相邻控制区 `polygon` 不得重叠，`polygon` 不与 `walls.yaml` 线段冲突、端边处通道宽度 ≥ 车宽 + 余量。

**每侧多个等待点的分配**：默认组内 `argmin dist` + validator 点间距校验（同侧相邻点间距 ≥ 车宽 + `d_queue_spacing`）；备选 `queue_seq % N_side`（`TrafficResponse.msg` 已有 `queue_seq`，**不新增字段**）。AMR 侧由独立 `traffic_gate_loader` 读取本文件（TBD-039）。

##### 3.2.1.4 工具链与校验（map_tool 为新增，不是同步）

| 位置 | 改动 |
| --- | --- |
| `tool/tool_ws/.../traffic_gates_editor`（**新节点**） | **两阶段**：阶段 A「点 4 下录 zone → `size()==4` / 凸 / 点序连续 / 面积 > 0 → 终端填 `resource_id`（仅此一项）」；阶段 B「点 N 下录等待点（N ≥ 2）→ 工具按几何归档到 L1 / L2 外侧两组、每侧 ≥ 1、点在区域内拒绝重录、`yaw` 由长轴 `P2→P3` 派生 → 写 `traffic_gates.yaml` + Marker 刷新」。**复用 `ClickedPointReceiver`（`/clicked_point` 仅 x/y）/ `ZoneMarkerPublisher`（end 边实线 / 侧边虚线 / 两组等待点分色 / 长轴箭头）；不复用 `zone_editor` 节点本体 / `YamlZoneManager` / `ZoneInfo` / `selectZoneType()`** |
| `tool/tool_ws/.../data_define.h` + 新 manager | 新建 `TrafficGate{resource_id, polygon, wait_poses}` 数据结构与 `traffic_gates.yaml` 读写；**不改 `ZoneInfo` / `Location`；不扩展 zone/station type 菜单**（不加 `NARROW` / `TRAFFIC_WAIT`） |
| `tool/map_tool/validator.py` | 现状只查文件存在，**新增 7 条内容校验**：① `polygon.size()==4`、凸四边形、点序连续、面积 > 0；② 两条端边近似平行 / 与长轴近似垂直、间距 `L_pull` ≥ 拉长下限；③ 端边与 `walls.yaml` 不冲突、通道宽度 ≥ 车宽 + 余量；④ `wait_poses.l1` / `.l2` 各 ≥ 1 且位于对应端边外侧（上游）、不在 `polygon` 内、相邻点间距 ≥ 车宽 + `d_queue_spacing`；⑤ 端边到真实窄道出口净距 ≥ `min_release_clearance`；⑥ 区域不与分叉点之前的公共通道重叠（保 S6）；⑦ `resource_id` 全局唯一、非空 |
| `robot_ws/src/amr_behavior_manager/traffic_gate_loader.{h,.cpp}`（**新建**） | 读取独立 `traffic_gates.yaml` 的 4 点 `polygon` + `wait_poses`（TBD-039），供中心点 `point_in_poly` 进出边沿、`entry_side` 计算与等待点导航；**不改 V1 `zone_station_loader`** |
| `amr_interfaces/msg/MapInfo.msg` + 打包 / 下发 8 处链路 | `MapInfo.msg` 加 `traffic_gates_yaml`：`REQUIRED_WAREHOUSE_FILES` / `metadata.py` / `package.py` / `validator.py` / `map_manager.cpp::publishMapInfo()` / `amr_behavior_manager` MapInfo 回调 / 新 `traffic_gate_loader` 同步；下发的是**文件路径**（`YAML::LoadFile(...)`） |
| `data/warehouse_01/metadata.yaml` | 地图包新增第 4 文件并升级版本，引用 TBD-003 |

**必须配套**：`docs/requirements/srs.md` D-051/D-052（`interface-contract.md`、`edge-data-model.md` 同步；SRS 需项目所有者授权后修改）；地图包版本升级策略引用 TBD-003。

#### 3.2.2 流程要求

**已定案（2026-11-20 第五轮）**：运行时按需触发主模型 + 目标全程 D + 被拒后等待点重导航 + **固定 `T_retry = 10 s`**。一趟任务可能遇到 N 个窄道，但**没有任何预先排好的序列**；每个窄道一个独立状态机，谁先遇到谁先申请。

```text
任务开始：GoodsPointResolver(goods_id + task_type) -> 目标 D
          发 NavigateToPose(D)，此后目标全程不变（INV-06 / INV-13）

行驶途中（只读 feedback->current_pose；不写 goal / 不改 costmap / 不下发途经点；无 plan 订阅）：
  A. AMR 中心点 inside false->true（进 4 点控制区边沿，滞回确认）
       -> 边沿时刻算一次 entry_side / wait_pose（dot(enter_pose-centroid, long_axis) 符号 + 组内 argmin dist，锁存）
       -> 发 TrafficRequest(request_id, robot_id, resource_id, ETA, lease_ttl)；不动 goal；IDLE -> REQUESTING
  B. REQUESTING 收到 GRANTED（S1 正常路径）-> 【零动作】：车继续沿 NavigateToPose(D) 行驶，
       不减速、不重发 goal、零停顿；REQUESTING -> HELD                          ★
  C. REQUESTING 收到 REJECTED / PENDING 超时（异常路径）
       -> cancel 当前 Nav2 goal -> 等 result 回调 m_goal_active=false
       -> 发 NavigateToPose(同侧就近等待点) -> 停稳进 WAITING
       -> WAITING：Nav2 无活动 goal、cmd_vel 闸门归零、每 T_retry = 10 s 用同一 request_id 幂等重发
  D. WAITING 收到 GRANTED（S2）-> 立即 navigateToPose(D)（事件驱动，不等轮询）-> RESUMING -> HELD

离开控制区：中心点 inside true->false（出区边沿，含掉头从入口退出）
       -> 立即释放路权（车不停，见 3.2.3）；改道驶离由出区边沿、停住 / 断流由 TTL 承接
```

> **★ 是「拉长控制区自带提前量 + 进区边沿即申请」的价值**：提前量由 `L_pull` 侧边拉长静态提供（`D_trigger` 已废弃、**无 `plan` 订阅**），进区边沿发申请不是「多一个动作」，而是让**绝大多数情况走「授权到 → 零动作」分支**。只有真被拒才付「cancel + 到同侧就近等待点 + 重发 D」的代价。上一版写成「预申请 N(i+1)」容易让人误以为要发新 goal，此处明确纠正。
>
> **链路、时序与实现规格补图（2026-11-20 第六 / 七轮）**：本节只定义「做什么」，**没有定义「谁连谁、走哪条 topic、谁调谁」**。实现者必须先读 [V2 交通路权控制链](v2-traffic-control-chain.md) §4〈数据链路逐跳表〉与 §5〈取消导航完整调用链〉、[V2 实现规格](v2-implementation-spec.md)（消息字段 / 状态机 / 伪代码 / 全场景流程），并对照 [进程边界与数据流图](../../assets/v2-traffic-dataflow.svg)、[五场景时序图](../../assets/v2-traffic-sequence.svg)。**模块归属与 ROS2 / MQTT topic 命名已于第七轮定案（INV-14），不再是「推荐 · 待冻结」**；仅表结构与数值参数仍属 TBD-020 / 021 / 022。

- **「发 goal 到等待点」不算写死路线**：等待点是**安全停车位置**，不是路径途经点；车到点即停、不按序列续走；授权后重发的是**最终目标 D**，不是「下一段」。全程不 Waypoint 化、不把 `B→C→D` 塞进导航目标。
- **不使用 `NavigateThroughPoses`**（项目所有者 2026-11-20 确认）：该 Action 虽在 `nav2_params.yaml:89/104` 已注册，但**没有业务卡口**，且每个 waypoint 会经 goal_checker 减速微停；本质是「下发有序途经点」，与本模型直接冲突。**V2 不引入该接口。**
- **`ComputePath` / `FollowPath` 独立 Action Client 已定案不引入**：按需模型里起点是「未来尚未到达的位姿」、目标始终是 D，概念上没有可作为独立规划锚点的「段」；AMR 自己调 `ComputePathToPose` 会与 `bt_navigator` 形成**两套路径真值**。**V0.20 起 `plan` topic 订阅也一并省掉**（提前量由拉长控制区的 `L_pull` 侧边静态提供，`D_trigger` 转译为 `L_pull`）。连带删除：`IsPathValid` / `path_validity_timeout` / 路径缓存 / 弧长排序 / 序列复核 / 预申请撤销 / `onPlan()` / `onPlanChanged()` / `pathIntersects*` / 改道去抖变量（TBD-038 / 039 已按此收窄，见第 9 节）。
- **等待时 Nav2 无活动 goal**：本就**无 `plan` 订阅**，等待期间不做路径预判；「重发 D 后是否绕行」只能在重发之后才知道。改道释放**保留语义、移除独立机制**：改道后驶离控制区 → 由「出区边沿」释放；停住 / 位姿断流 → 由 `lease_ttl` 到期 `EXPIRED` 回收；任务结束 / 显式 `CANCEL` → 四触发取最早（见 3.2.3）。
- **`T_retry = 10 s` 固定值**（项目所有者 2026-11-20 定案，**不加 jitter**）：主路径是事件驱动（`response/{robot_id}` 单播订阅为 **V2 新增**，V1 仅预留 topic 常量、两侧 handler 注册被注释，见[控制链 §2.3](v2-traffic-control-chain.md)），一收到 response 立即重发 D、**不等下一次轮询**；轮询只作丢包 / 拒答兜底，重发沿用同一 `request_id` 幂等。固定 10 s 带来的「多车同步申请」由 **Edge 侧 FIFO 排队 + 幂等去重**吸收（3.2.5），不再靠客户端 jitter 打散。
- **申请带 ETA 与 `lease_ttl`**：ETA 供 Edge 排队与 lease 生效时刻参考；`lease_ttl` 是本次期望的**权限占用时限**，Edge 授权时写入 `lease_expires_at`，到期自动回收（3.1.2 / 3.2.5）。具体数值由 TBD-038 确定。
- **Topic 与 QoS**：沿用 `amr/traffic_rights/request`、`response/{robot_id}`，新增 `cancel` 动作；Edge 侧按动作分层 + 通配符订阅（3.2.5）。QoS 1，`request_id` 幂等。路权占用状态**不得广播给其他 AMR**（`AGENTS.md`）。
- **不改 `task_state`**：一趟任务一个 `task_state`，路权只是沿途按需遇到的闸门（INV-13）。

#### 3.2.3 与 Nav2 的协调

- **目标全程 = D**：`NavigateToPose` 的目标始终是最终目的地 D，唯一例外是 3.2.2 分支 C「被拒时临时发到等待点」。释放与导航结果解耦，不因释放而改 goal。
- **模型 A 已定案**：等待期间 **Nav2 无活动 goal**，`cmd_vel` 由 3.5 闸门归零；因此 `Failed to make progress` 风险**由结构消除，不靠调参**。`nav2_params.yaml:125-128` 的 `movement_time_allowance: 20.0` 且 `SimpleProgressChecker` 只统计平移，任何「保持活 goal + 速度归零」的方案超过 20 s 都会触发 recovery 直至导航失败，多车排队常态超时——**实现者不得采用该模型**（决策留痕见 `.workbuddy/artifacts/navigation-continuity-review.md` §4.1）。
- **释放条件已定案（四个触发，取最早者，缺一不可）**：

| 触发 | 场景 | 为什么必须 |
| --- | --- | --- |
| ① **中心点出区边沿**（`inside` true→false，主判据） | 正常通过 / 掉头从入口退出 / 改道驶离控制区 | 车不停、释放及时；方向无关，与进区边沿同一个 `point_in_poly` + 滞回。旧「越外移释放线」与「反向跨 trigger 无退出分支」缺口在此自动消失 |
| ② 导航 `SUCCEEDED` / 任务结束 / 异常 `ABORTED`（兜底） | 出区边沿漏检、D 恰在控制区附近 | 绝不漏释放；**从上版唯一判据降级为兜底**，不改变「正常路径靠空间事件」的主次 |
| ③ **`lease_ttl` 到期由 Edge 自动回收** `EXPIRED`（兜底） | 车停在区内不动 / 位姿断流 / 改道后驶离但边沿漏检 / AMR 崩溃 / 取消消息丢失 / 通信中断 | 项目所有者本轮明确「申请时有权限占用时限」。**这是「怎么知道 AMR 取消了申请」的最终答案**：不必知道，快路径显式 `cancel`，安全网靠时限 |
| ④ **显式 `CANCEL`**（一等事件） | 任务取消 / 放弃等待 / 排队中出队 | 带 `request_id` + `epoch`，Edge 按状态分「出队 / 释放」两态；不得靠静默 / 断线推断（见 3.2.5） |

- **「改道即释放」保留语义、移除独立机制**：改道后驶离控制区 → 由「出区边沿」（触发 ①）释放；停住 / 位姿断流 → 由 `lease_ttl`（触发 ③）；任务结束 / 显式 `CANCEL` → 触发 ② / ④。**不再有 `plan` 订阅 / `onPlanChanged()` / `pathIntersects*` / 「`plan` 不再与窄道相交 + 位姿远离 + 连续 N 次去抖」/ 改道冷却** 等实现（已废弃）；`zonePolygon()` 保留供 validator / 异常裁决 / 可视化，只是不再被改道检测调用。四触发取最早，权限不会被占死到对向车白等。
- **不做运行时 `footprint` / 车尾判定**（红线保留）：主判据是 **AMR 中心点** `point_in_poly` 的 `inside` 边沿（单点、边界唯一），不是 `footprint`（车体轮廓 N 个角点会放大位姿噪声、翻转窗口更宽）、也不是跨线事件。端边外移 `min_release_clearance` 把「中心出区 ⇒ 车尾已出窄道」变成**录入时一次性静态几何约束**（TBD-021）；边界抖动由**边沿检测 + 滞回双阈值**（TBD-038）处理，而非逐帧 `footprint`。
- **先到释放点不采用**：只要释放点是独立 `NavigateToPose` 目标，物理上必然走「减速 → 停 → `RotateToGoal` 收敛 yaw → `trans_stopped_velocity 0.25` 判停稳 → `general_goal_checker` 判 `SUCCEEDED` → 重发 D → 起步」完整停顿链，与 `NavigateThroughPoses` 的 waypoint 减速同源。项目所有者判断正确，**V2 不采用**。
- **资源生命周期 ≠ 导航 Action 生命周期**（INV-13）：释放可发生在导航仍在进行中，不得把「释放路权」绑定为「导航任务结束」，也不得因此重启整套导航流程。
- 拿到路权后若导航失败，走 BT 的 `Retry` / `Fallback`，并**释放路权**，避免占着路口不动。
- **实施前必须先修 4 个现有缺陷**（否则「cancel + 重发 goal」频繁发生会放大）：`navigation_server.cpp:307` 无条件 `ACCEPT_AND_EXECUTE` 的 supersede、`:456-470` 60 s 结果超时只 abort 不 cancel Nav2 goal、`:344/372/442` `m_nav_epoch` 竞态与无锁共享状态、`navigation_manager.cpp:58-65` `accepted` 语义混淆（应为 `submitted`）。详见评审意见稿 §4 / §10.6；本轮为**阻塞项**，列入 3.7 第 4 步。

#### 3.2.4 异常路径（必须实现）

| 场景 | 要求 |
| --- | --- |
| 申请超时无响应 | 按固定 `T_retry = 10 s` 用同一 `request_id` 幂等重发；超过上限上报 `CARRY_ABORTED`，原因码新增需同步契约 |
| 持权 AMR 掉线 | 心跳超时（5 分钟）或 MQTT Last Will 触发 → 自动回收租约；取消消息丢失时由 `lease_ttl` 到期回收 |
| Edge 重启 | 按 3.1.5 重建并重发授权；`PENDING` 队列须一并重建或显式作废，不得留下幽灵排队项 |
| 两车同时申请同一路口 | Edge 按 `queue_seq` FIFO 仲裁，只有一个获批，另一个进 `PENDING` 等待；不得同时放行 |
| **AMR 主动取消申请**（改道 / 任务取消 / 放弃等待） | 必须发**显式 `cancel`**（带 `request_id` + `epoch`）；排队中 → 出队，已授权 → 走 release。Edge **不得**靠静默 / 断线推断 |
| `lease_ttl` 到期 | Edge 自动回收并释放资源；若 AMR 仍在使用须重新申请，不得静默续租 |
| 持权期间任务被取消 | 释放路权 + 配额回退，与 AC-011 语义一致 |

#### 3.2.5 Edge 侧仲裁、排队与取消感知（本轮新增）

**已定案的原则**（项目所有者 2026-11-20 确认）：权限申请的事件机制由 **Edge 侧保证并形成排队**；AMR 侧只发事件、不做全局排序。

**为什么必须在 Edge**：只有 Edge 持有唯一的全局 lease 表；AMR 各自看不到对向车的申请，若在 AMR 侧按本地重试排序，两车会各自认为自己该先走。中心排队是 3.0.1「共享资源单点仲裁」的必然结果，不是额外架构。

**通配符订阅能解决什么、不能解决什么**：

```text
【已定案 · 2026-11-20 第七轮】单一上行 topic + payload op，不采用动作分层 topic
AMR → Edge : amr/{robot_id}/traffic_rights/request   payload.op = REQUEST | CANCEL | RELEASE
Edge → AMR : amr/{robot_id}/traffic_rights/response  单播，不广播
Edge 订阅  : amr/+/traffic_rights/request            ← robot_id 一层通配，新增机器人零配置
```

- ✅ 解决「接得到所有机器人的消息」、新增机器人无需改订阅列表（`robot_id` 用 `+`，与 V1 `amr/+/status` 同一范式）。
- ✅ **动作语义由 payload `op` 承载**：`REQUEST` / `CANCEL` / `RELEASE`；Edge 一个 handler 按 `op` 分发。**不采用动作分层 topic**（不再有 `.../cancel`、`.../release` 各一条），理由是减少 handler / 订阅项，并让「取消是一等显式消息」在 payload 层可审计。
- ⚠️ **禁止**把三种动作塞进同一 topic 却不带 `op`；`op` 为必填字段，缺失即丢弃并记日志。
- 无论哪种形态，payload 至少带 `request_id`、`resource_id`、`robot_id`、`epoch`、`op`；`robot_id` 须与 topic 中的 `{robot_id}` 交叉校验，防串台 / 伪造。
- 响应仍走**单播** `amr/{robot_id}/traffic_rights/response`，不广播；路权占用状态不得下发给其他 AMR（`AGENTS.md`）。
- **V1 常量须替换**：`TRAFFIC_RIGHTS_REQ = "amr/traffic_rights/request"` 缺 `robot_id` 层，与 `amr/+/traffic_rights/request` 四级结构不匹配。V2 改为 `AMR_TRAFFIC_REQUEST = "amr/{}/traffic_rights/request"`，并补 `makeAmrTrafficRequestTopic(robot_id)` / `makeAmrTrafficResponseTopic(robot_id)` helper（见实现规格 §1.2）。

**「怎么知道 AMR 取消了申请」—— 三条硬约束**：

| # | 约束 | 原因 |
| --- | --- | --- |
| 1 | **取消是一等显式消息**，禁止靠「不再重试 / 断线 / 静默」推断 | AMR 可能只是没到 10 s 轮询点、网络抖动，或 V1 待补报链路仍在；静默有多种合法含义 |
| 2 | **幂等 + 陈旧丢弃**：`request_id` 去重，`epoch` 丢弃迟到消息 | QoS 1 会重传；导航被 `cancel` / supersede 后旧消息可能晚到 —— 与 AMR 侧 `m_nav_epoch` 竞态（3.2.3 阻塞项）同源，Edge 侧同样要防 |
| 3 | **TTL 是安全网**：`lease_ttl` 到期自动回收 | AMR 崩溃、`cancel` 丢包、车停在区内不动 / 位姿断流、改道后驶离但出区边沿漏检 —— 这些情况 Edge 永远等不到显式取消，靠时限兜底（3.2.3 触发 ③） |

> **「申请带占用时限」与「如何感知取消」是同一个机制的两面**：项目所有者本轮定的 `lease_ttl`，正是取消感知问题的最终兜底。显式 `cancel` 是快路径，TTL 是安全网；**Edge 不需要可靠地「知道」AMR 取消，也能保证资源不被占死**。

**lease 状态机与两种取消语义**：

```text
  PENDING（排队中，不占资源）
     │ 原子授权（部分唯一索引仅覆盖 GRANTED/HELD）
     ▼
  GRANTED ──> HELD ──> RELEASED
     │           │          ▲
     │           │          └─ ① 出区边沿 / ② SUCCEEDED / ③ TTL / ④ 显式 CANCEL
     │           └─ 显式 cancel（已授权）──┘
     └─ 显式 cancel（排队中）-> CANCELLED（出队）
  任意态 ── lease_ttl 到期 / 心跳超时 ──> EXPIRED（自动回收）
```

- 排队中取消 = **出队**，不影响已授权的车；已授权后取消 = **释放 lease**，须回退配额（3.1.2 约束 7）。
- 收到带旧 `epoch` 的 `cancel` / `request` 一律丢弃并记录，不得回退状态。

**复杂度边界（回应「会大大提升复杂度」）**：V2 只做上表五件事（单资源 FIFO、显式 `cancel`、幂等 + `epoch`、TTL 回收、动作可辨的订阅）。**不做**优先级抢占、多资源联合预约、死锁检测与回滚、AMR 侧排队状态广播、按 ETA 动态插队；这些推迟到 V3 及以后。面试时主动说明「中心排队会引入迟到消息 / 幂等 / 超时回收三类问题，我用边界收敛而不是加机制去覆盖」，比堆机制更值钱。

**命名与形态已冻结（2026-11-20 第七轮）**：topic 名（INV-14）、动作承载方式（payload `op`，不分层）、模块归属、消息字段集合 —— 实现者**不得再自行选择**，详见 [V2 实现规格 §1](v2-implementation-spec.md)。

**仍待 TBD-020 冻结**（实现者不得自行选择）：`queue_seq` 生成方式、`PENDING` 超时作废时长、`lease_ttl` 默认值与续租规则、SQLite 表最终字段与是否新增 `tbl_resource_lease`、与 `tbl_task_goods_allocation` 的一致性策略。

---

### 3.3 电量管理

#### 3.3.1 重要事实：链路已存在，只缺真实数据源

V1 端到端已经打通，V2 **不需要新建上报链路**：

| 环节 | 现状 |
| --- | --- |
| AMR 上报 | `robot_ws/src/amr_agent/src/amr_agent_node.cpp:296` → `root["battery"] = 100.0;`（**硬编码**） |
| 行为侧 | `robot_ws/src/amr_behavior_manager/src/amr_behavior_manager_node.cpp:562` → `rs.battery = 100.0f;`（**硬编码**） |
| Edge 解析 | `edge_server/src/message/message_codec.cpp:108` → `status.battery = root.value("battery", 0.0);` |
| Edge 入库 | `sqlite_db.cpp:711` → `battery=excluded.battery`；`tbl_robot_status.battery REAL` 字段已存在 |
| Edge 读取 | `sqlite_db.cpp:803` → `status.battery = sqlite3_column_double(stmt, 6);` |

因此 V2 的工作是：**移除两处硬编码，由 AMR 侧 `amr_power_manager` 接管 SOC 真值与积分，Edge 侧仅在仲裁层加入 `CHARGE_POINT` 授权与只读镜像**。这比「从零做电量管理」小得多，也印证了「V1 已有地基」的判断。

> **归属更正**：本节所述全部电量计算逻辑均位于 AMR 侧 `amr_power_manager`（`robot_ws`），不在 Edge。依据与职责红线见 3.0 / 3.0.1。

#### 3.3.2 电量模型（B 方案已冻结、TBD-036 已冻结为 D；速率数值待 TBD-022）

**决策状态**：充电形态由项目所有者确定为 **B 方案**，消耗档位判定信号确定为 **D 方案**——`amr_power_manager` 本地按仿真时间速率积分并累加，**不引入独立充电模拟器 HTTP 服务、不引入远端充电源、V2 不实现 `ChargeSource` 接口抽象**（决策记录见 3.3.5）；档位由 `amr_behavior_manager` 通过专用 `PowerMode` 单向声明，电量层零判定、零 `cmd_vel` 订阅（决策记录见 3.3.2.2，落地细则见 3.3.2.3）。消耗与充电统一采用「按仿真时间的速率」模型，与 B 的语义对称，配置项可直接对照。

**单位与语义约定（必须先统一）**：`battery` 字段为 SOC 百分比（0.0 ~ 100.0，与 V1 硬编码 `100.0` 一致）。V2 采用**速率模型**：各速率单位为 `SOC% / min`，乘 `dt_sim` 折算；**不引入 Wh / W 功率量纲**，也不做「一次事件加一个固定值」的离散累加。

**消耗模型（两档，D 方案已冻结）**：`mode` 来自行为层声明的 `PowerMode`，`amr_power_manager` **不做任何判定**，只按当前档位查表取速率。

| 档位 | 判定条件 | 配置项 | 说明 |
| --- | --- | --- | --- |
| `MODE_IDLE` | 行为层声明非执行态（等货 / 等路权 / `INTERRUPTED`） | `idle_drain_pct_per_min` | 待机功耗：控制器、传感器、通信 |
| `MODE_WORK` | 行为层声明执行态（含 `NAVIGATING_HOME`） | `work_drain_pct_per_min` | 运动与作业功耗，应显著大于 IDLE |

```text
// mode 来自行为层 PowerMode 声明，电量层不判定；CHARGING 时走充电模型，不走本式
rate = (mode == WORK) ? work_drain_pct_per_min : idle_drain_pct_per_min
dSOC = - rate * dt_sim / 60
```

**充电模型（B 方案，已冻结）**：

```text
dSOC = + charge_pct_per_min * dt_sim / 60     // 仅当充电前置条件全部满足
```

##### 3.3.2.1 档位判定信号：不订阅上层状态（TBD-036 已冻结为 D）

项目所有者提出「`amr_power_manager` 看不到 AMR 状态」。核查后需先纠正前提，再给取舍。本节保留三条「不订阅上层状态」的实证理由（D 方案同样成立），并给出最终冻结的 D 结论；曾评估但已否决的 `cmd_vel` 方案见 3.3.2.2，落地细则见 3.3.2.3。

**前提纠正**：状态其实可见。`amr_behavior_manager_node.cpp:542-570` 的 `publishRobotStatus()` 已在每机器人 namespace 下发布 `robot_status`（`amr_interfaces/msg/RobotStatus.msg`），其中含 `state`（`STATE_IDLE / EXECUTING / CHARGING / ERROR / SAFE_MODE`）与 `navigating`，另有 `task_status`、`component_status`。`amr_power_manager` 与它同属 AMR 侧 ROS2 图，**订阅不需要新增任何跨进程接口或协议变更**。

**但仍不建议用它作为档位主信号**，三条实证理由：

1. **语义粒度不匹配**：`publishRobotStatus()` 里 `STATE_EXECUTING` 由 `NAVIGATING_TO_PICKUP / GRASPING / TRANSPORTING / PLACING` 四个 `task_state` 聚合而成，而 V1 的 `GRASPING` / `PLACING` 是**固定延时、车静止**（D-045），等待路权、等待货物批准同样静止。一律套 WORK 档会在车不动时虚高扣电。`navigating` 更精确，但覆盖不了这些静止阶段。
2. **数据回流成环**：`robot_status` 自带 `battery` 字段，且当前在 `amr_behavior_manager_node.cpp:562` 硬编码 `100.0f`。V2 正要让 `amr_power_manager` 提供这个真值；若它反过来订阅 `robot_status` 判档，就变成「消费一条包含自己输出的消息」，形成双向依赖，上层状态机异常或发布延迟会直接污染电量模型。
3. **依赖方向倒置**：BMS / 电量估计在软件栈最底层，真实系统读电流传感器，不读任务编排层；且 INV-12 要求「模块之间通过明确接口交互，不直接依赖其他模块内部实现」，`STATE_EXECUTING` 的聚合规则正是 behavior manager 的内部语义。让底层依赖上层，断网、BT 重构或状态机故障时电量会一起失效，与 V1 主打的断网续行自洽性冲突。

**最终决策（TBD-036 已冻结为 D）**：档位**不由电量层判定**，而由 `amr_behavior_manager` 用**单向、不含 `battery` 的专用 AMR 内部 topic `power_mode`** 主动声明 `MODE_IDLE` / `MODE_WORK` / `MODE_CHARGING`；`amr_power_manager` 只订阅该 topic、缓存 `mode`、查表积分，**零判定逻辑、零 `cmd_vel` 订阅、不订阅 `robot_status`**。接口、发布时机、失败回落与文件级改动见 3.3.2.3。

- **为什么弃用 `cmd_vel` 方案**：物理速度只回答「车在不在动」，不回答「这趟算不算工作」；抓取 / 放置 / 等路权 / 等货物批准时车静止但仍在任务中，需要时间窗 + 滞回 + `move_threshold` + mux 切换点一整套补丁去兜，成为全项目最重的一块判定逻辑；且 V4 `GraspAction` 期间底盘锁止、`cmd_vel == 0`，会把机械臂执行器耗电漏算为 IDLE。完整评审见 3.3.2.2。**实现者不得实现 `mode_window_sec` / `move_threshold` / `cmd_vel` 订阅。**
- **为什么弃用「订阅上层状态」**：即上方三条理由——尤其 `robot_status` 自带 `battery`，订阅它会形成「消费一条包含自己输出」的回流环。D 用专用、不含 `battery` 的 `PowerMode`，从消息结构上就杜绝了回流。
- **多机 namespace**：`power_mode` / `power_level` 一律用**相对 topic**，由 launch namespace 解析为 `/{robot_id}/...`，**禁止写绝对名**。此坑与 `troubleshooting-dynamic-obstacle.md` 记录的「相对话题名在子命名空间下被解析成 `/robot01/local_costmap/scan`」同源。

> **为什么这么定**：「这趟算不算工作」本就是行为层定义的语义，由其声明、电量层只做积分，职责最清晰；D 与「底层依赖上层」的方向相反，是**行为层主动声明 → 电量层消费**的单向事件，因此不触犯上方三条理由。运行时判定几乎清零，换来两个静态 msg + 两个常量（一次性、易审查）；同套抽象与 3.4.3「Grasp 占位叶节点 V4 只换实现、父树不动」一致。给定 `PowerMode` 序列与配置即可离线复算 SOC 曲线，V1.5 基准实验可直接断言。

##### 3.3.2.2 评审记录：复杂度与 V4 机械臂盲区（TBD-036 已冻结为 D）

项目所有者对原 `cmd_vel` 判档方案提出两点质疑，评审后**两点均成立**，据此收敛到 D。**TBD-036 已由项目所有者冻结为 D 方案**；本节作为决策留痕保留（讲清「为什么没那么做」），正文、验收清单与改动清单均已按 D 同步，实现者以 3.3.2.3 为唯一落地依据。

**① 复杂度的根源是「用物理量猜业务语义」**。3.3.5 已把 B 方案定位为「刻意保持线性可复算，把复杂度留给资源仲裁与行为树」，但曾评估的 `cmd_vel` 备选方案却给电量模块加了订阅 + 时间窗 + 滞回 + `move_threshold` + mux 切换点 + 相对 topic 约束，会成为全项目判定逻辑最重的一块。`cmd_vel` 表达的是「车在不在动」，而要判的是「这趟算不算工作」；两者不重合（抓取、放置、等路权、等货物批准时车静止但仍在任务中），才需要上述补丁去兜。**这属于过度设计，应简化。**

**② `cmd_vel` 对 V4 机械臂确实存在盲区**。`v4-mobile-manipulation.md` 3.3 / 3.5 规定 `GraspAction` 执行期间底盘锁止、`cmd_vel == 0`，按 `cmd_vel` 判据会被算成 IDLE 档，执行器耗电被低估。评审同时发现 V4 分册原**完全没有电量 / 抢占策略相关表述**，是一处真实缺口，现已按 TBD-037 登记（见 V4 分册 3.6）。

**候选收敛（TBD-036 已拍板：采用 D）**：

| 候选 | 做法 | 运行时逻辑 | 新增接口 | V4 扩展 | 结论 |
| --- | --- | --- | --- | --- | --- |
| 原方案 | 订阅 mux 后 `cmd_vel` + 时间窗 / 滞回判档 | 重 | 无 | 需再加订阅或附加项 | **否决** |
| **D** | 由 `amr_behavior_manager`（V2 起即 BT）通过**单向、不含 `battery` 的 AMR 内部 topic** 声明 IDLE / WORK / CHARGING；`amr_power_manager` 只查表积分 | **无判定逻辑** | 2 个 msg + 2 个 topic（须在接口契约登记） | `GraspAction` 叶节点 emit 一个附加档位即可，父树 / FSM / 电量模块不动 | **采用（已冻结）** |
| E（最省） | V2 取消两档，统一 `drain_pct_per_min` 一档 | 无 | 无 | 同 D | 否决（保留两档以体现工况差异） |

**D 与上一节反对理由并不冲突**：3.3.2.1 反对的是订阅 `robot_status`（该消息自带 `battery`，会形成数据回流）与读取上层内部聚合语义；D 是「行为层主动声明 → 电量层消费」的**单向事件**，方向相反。「这趟算不算工作」本就是行为层定义的语义，由其声明、电量层只做积分，职责更清晰；且与 3.4.3「Grasp 占位叶节点 V4 只换实现、父树不动」是同一套抽象。

**V4 真正该补的不是耗电精度，而是抢占策略**：BT 的 `ReactiveFallback(BatteryOK?)` 是抢占式的，机械臂抓取途中电量跌破阈值时是否允许打断臂动作，比少扣 0.x% 电量重要得多，且与 V4 分册 3.2.3「急停必须能中断臂动作并回安全位」属同一类问题。B 方案是分钟级仿真简化模型，几秒级的执行器动作占比极小，**V2 不为 V4 预留运行时判定机制**（D 的扩展只需加 1 个枚举 + 1 行配置，见 3.3.2.3（7））；该缺口已随 TBD-036 关闭登记为 **TBD-037**，落在 V4 分册 3.6。

**必须满足的建模约束**：

1. **时间基准**：`dt_sim` 取自仿真时钟（`use_sim_time=True`），**禁止使用 `std::chrono` / 墙钟定时器**。Gazebo 可暂停、可加速，墙钟会导致电量与仿真时间脱节。此坑与 `nav2-tuning-notes.md` 第 4 节记录的 `/clock` / TF 时间同步风险同源。
2. **SOC 边界**：每次积分后 clamp 到 `[0, 100]`；充到 100 停止累加、放到 0 不再下降，不得溢出或为负。
3. **滞回双阈值**：`low_soc_threshold`（触发回充，建议 20）与 `charge_target_soc`（停止回充，建议 80）必须分离且有最小间隔，防止阈值附近反复「接任务 / 回充」震荡。
4. **充电前置条件**（全部满足才允许声明 `MODE_CHARGING` 并积分）：到达 `type=IDLE` 的 location + 停稳 + 持有 `CHARGE_POINT` lease + 接触确认。这些由**行为层 / 仲裁层在声明前保证**，电量层不判 `cmd_vel`、不判位置、不判 lease；任一条件丢失立即由行为层改声明 `MODE_IDLE`/`MODE_WORK`，电量层随即停止积分并上报。
5. **中断与释放**：充电中被抢占 / 急停 / lease 超时 → 行为层改声明、电量层停止积分、释放 lease、状态回到 V1 已有的 `task_state` 枚举（不得新增枚举）。
6. **消耗与充电互斥**：同一 `dt_sim` 内只允许一种速率生效，D 下由 `PowerMode.mode` **单值天然保证**（CHARGING / WORK / IDLE 三选一），不需额外优先级代码；不得同时加减。
7. **数据落库**：是否新增 `tbl_robot_battery_log`（用于画电量曲线、断言与基准实验）由 TBD-022 决定；`battery` **不得**并入 `goods_status` 等共享状态（避免污染 INV-07）。
8. **可验证**：给定 `PowerMode` 序列与配置可离线复算 SOC 曲线，V1.5 基准实验可直接断言。
9. **可选增强，V2 不实现**：CC-CV 非线性充电曲线、充电效率 `eta`、连续功率模型（按实际电流/功率而非固定档位速率积分）、载重系数。B+D 方案刻意保持「档位查表 + 线性可复算」；若后续要升级须走 TBD-022 并同步本分册，**实现者不得自行「补上」曲线，也不得把 `cmd_vel` 或电流量纲接回电量模块**。

> **为什么这么定**：B 方案的价值是简单、确定、可复算。刻意不引入功率量纲与曲线，让实现者用最少代码跑通全部验收项，同时把「为什么不按上层状态扣电」这条取舍讲清楚；`dt_sim` 与 clamp 是唯二不能省的硬约束——前者保证与仿真时间一致，后者保证数值不越界。

##### 3.3.2.3 D 方案落地细则（**TBD-036 已冻结为 D · 实现者唯一落地依据**）

> 本小节是 3.3.2.2 候选 D 的**实现级细化**，已由项目所有者冻结为 V2 电量链路的落地依据；3.3.2.1 / 3.3.2.2 为决策留痕与理由，具体实现以本节为准。上方的「必须满足的建模约束」属 3.3.2 主体，`dt_sim` / clamp / 双阈值 / 充放电互斥沿用不变；与 `cmd_vel` 判档相关的条目在 D 下取消。

**一句话原理**：`amr_behavior_manager` 用**一个专用、不含 `battery` 的 AMR 内部 topic** 声明「现在按哪个速率扣电」，`amr_power_manager` 只做「查表 + 乘 `dt_sim` + clamp」，**零判定逻辑、零 `cmd_vel` 订阅**。

**（1）数据流**

```text
amr_behavior_manager ── PowerMode ──▶ amr_power_manager ── BatteryState ──▶ amr_behavior_manager
                                          │                                      │
                                    SOC 真值 + is_low                   ① 填 robot_status.battery
                                    （唯一持有者）                        ② BT 的 BatteryOK 条件
                                                                                │
                                                                                ▼
                                                                    amr_agent（已有 robot_status 订阅）
                                                                                │
                                                                                ▼
                                                          5s MQTT 上报 → Edge tbl_robot_status.battery
```

**关键**：`PowerMode` 不含 `battery`，且 `amr_power_manager` **不订阅** `robot_status`，因此不存在 3.3.2.1「三条不订阅上层状态理由」第 2 条的「消费一条含自己输出的消息」回流环。D 与「订阅 `robot_status` 判档」的本质区别是方向：**行为层主动声明 → 电量层消费**，不是底层去读上层聚合状态。

**（2）接口清单**（新增 2 个 msg + 2 个常量；其余全部复用 V1）

| 类别 | 名称 | 文件 | 说明 |
| --- | --- | --- | --- |
| 新增 msg | `PowerMode.msg` | `amr_interfaces/msg/` | 入站，行为层 → 电量层 |
| 新增 msg | `BatteryState.msg` | `amr_interfaces/msg/` | 出站，电量层 → 行为层 |
| 新增常量 | `POWER_MODE = "power_mode"` / `POWER_LEVEL = "power_level"` | `amr_common/ros_interface_names.h` | 项目规则：ROS2 名集中登记 |
| 注册 | `rosidl_generate_interfaces` 加 2 行 | `amr_interfaces/CMakeLists.txt` | `package.xml` 依赖不变（`builtin_interfaces` 已有） |
| 复用 | `robot_status`（`RobotStatus.battery`） | 已有 | battery 分发通道，**不给 `amr_agent` 加订阅** |
| 不变 | `task_state` / 5s 上报 / `status/ack` / 待补报 / MQTT 字段 | — | 全部冻结，零改动 |

`PowerMode.msg`：

```text
# 行为层 → 电量层的单向活动档位声明（每机器人 namespace 下 topic: power_mode）
int32 MODE_IDLE=0
int32 MODE_WORK=1
int32 MODE_CHARGING=2
int32 mode
builtin_interfaces/Time stamp
```

`BatteryState.msg`：

```text
# 电量层 → 外部的唯一真值输出（每机器人 namespace 下 topic: power_level）
float32 battery          # SOC 0~100
bool is_low              # 滞回后的低电量标志，供 BT BatteryOK 消费
builtin_interfaces/Time stamp
```

> **枚举刻意只有 3 个值**：V2 不为 V4 预留 `ARM_ACTIVE`，见（7）。

**（3）发布时机：复用现有 1 Hz 定时器，零新增定时器**

`amr_behavior_manager_node.cpp:144` 已有 `status_publish_rate_hz=1.0` 的 `publishRobotStatus()` 定时器，并在其中算出 `actionActive`。D 只在该函数里**多 publish 一条 `PowerMode`**，不新增定时器、不做边沿触发、不配 `transient_local`：

| 条件（按优先级） | `mode` | 说明 |
| --- | --- | --- |
| `m_charging == true`（V2 由 BT `GoCharge` 分支置位） | `MODE_CHARGING` | 到位 + 停稳 + lease + 接触确认**全部由行为/仲裁层保证后才置位**，电量层不再判 `cmd_vel` |
| `m_task_state ∈ {NAVIGATING_TO_PICKUP, GRASPING, TRANSPORTING, PLACING, NAVIGATING_HOME}` | `MODE_WORK` | 现有 `actionActive`（556-560 行）**漏了 `NAVIGATING_HOME`**，D 需补上，否则返航不扣电 |
| 其余（`IDLE` / `ACCEPTED` 等货 / `INTERRUPTED`） | `MODE_IDLE` | 车静止、不扣工作档 |

**为什么周期发而不是状态变化时发**：① 天然心跳，省掉「`amr_power_manager` 晚启动错过消息」的启动顺序问题；② 不用 `transient_local` QoS 与边沿逻辑；③ 1 秒档位延迟在分钟级速率下可忽略。项目里 `transient_local` 只用于地图 / 初始位姿这类一次性数据（`map_manager.cpp`），档位不属于此类。

**（4）`amr_power_manager` 内部逻辑（伪代码）**

```text
on PowerMode(msg):
    m_mode = msg.mode                      // 只存，不判定
    m_last_mode_stamp = this->now()        // ROS time

on tick(例如 10 Hz wall_timer，仅作触发):
    now = this->now()                     // ROS time, use_sim_time=True
    dt_sim = (now - m_last_tick).seconds()
    m_last_tick = now
    if (now - m_last_mode_stamp > mode_timeout_sec)  m_mode = MODE_IDLE   // 保守回落

    if (m_mode == CHARGING) rate = +charge_pct_per_min
    elif (m_mode == WORK)   rate = -work_drain_pct_per_min
    else                   rate = -idle_drain_pct_per_min

    m_soc = clamp(m_soc + rate * dt_sim / 60, 0, 100)
    m_is_low = hysteresis(m_soc, low_soc_threshold, charge_target_soc)
    publish(BatteryState{ m_soc, m_is_low, now })
```

- **充电与放电互斥**由 `m_mode` 单值天然保证（约束 6），不需要额外优先级代码。
- **`dt_sim` 用 `this->now()` 而非墙钟差值**：定时器可用 `create_wall_timer` 触发（与项目现有风格一致），但 `dt` 必须取 ROS time。Gazebo 暂停时 `this->now()` 不前进 → `dt≈0` → 不积分；同时超时判据也用 ROS time，暂停期间不会误判为「消息丢失」。这正是 `nav2-tuning-notes.md` 第 4 节 `/clock` 坑的规避方式，也与 `amr_agent_node.cpp:131` 现有 watchdog 用 `this->now()` 一致。
- **`hysteresis`**：跌破 `low_soc_threshold`（建议 20）置 `is_low=true`，升到 `charge_target_soc`（建议 80）才置回 false，避免阈值附近反复回充。

**（5）充电闭环：不需要 `amr_power_manager` 判断「充满」**

```text
is_low=true → BT ReactiveFallback(BatteryOK?) 抢占 → GoCharge 分支
   → 到位/停稳/lease → 声明 MODE_CHARGING → 充电积分
   → SOC 升到 charge_target_soc → is_low=false → BatteryOK 恢复
   → BT 退出 GoCharge → 声明 MODE_IDLE/WORK → 充电停止、释放 lease
```

阈值、滞回、SOC 全在 `amr_power_manager` 单点；BT 只读一个 `is_low` 布尔。**谁都不需要知道对方的细节**，这是 D 比 `cmd_vel` 方案简单的地方。实施时需在 3.4.3 的 `BatteryOK` 条件节点注明数据源为 `BatteryState.is_low`。

**（6）失败模式与边界**

| 场景 | 行为 |
| --- | --- |
| `PowerMode` 丢消息 / 行为层挂掉 | 超过 `mode_timeout_sec`（建议 3 s，与 `amr_agent` 现有 3 s watchdog 一致）回落 IDLE，按最低速率扣电并记日志，不报错、不中断 |
| 急停 | 由 3.5 mux 归零 `cmd_vel`；行为层进入安全态后声明 `MODE_IDLE`。`amr_power_manager` 不订阅 `cmd_vel`，**但结果等价** |
| Gazebo 暂停 / `/clock` 跳变 | `dt_sim` 自动为 0 / 不跳变，SOC 不动 |
| 多机 namespace | `power_mode` / `power_level` 一律**相对 topic**，由 launch namespace 解析为 `/{robot_id}/...`；禁止绝对名（同 `troubleshooting-dynamic-obstacle.md` 的坑） |
| Edge 断网 | SOC 与档位都在 AMR 本地，不依赖网络；恢复后按 V1 待补报链路上报 |

**（7）V4 扩展点（V2 不实现、不预留）**

若 V4 要体现机械臂执行器耗电，**只需两处**：① `PowerMode.msg` 增加 `MODE_ARM_ACTIVE=3`；② `amr_power_manager` 速率表增加一行 `arm_drain_pct_per_min`。`GraspAction` 叶节点在 `tick()` 期间声明该档位即可。**不改 topic、不改消息结构、不改积分逻辑（`switch` 多一个 case）、不改父树、不改任务 FSM、不改 `amr_agent`**。这正是 3.4.3「动作可替换、编排不变」在电量链路上的兑现。

同时按 3.3.2.2 的结论，V4 分册需补 `ReactiveFallback(BatteryOK?)` 在臂动作中途的抢占策略（是否允许打断）；TBD-036 已关闭，该缺口已登记为 **TBD-037**，见 V4 分册 3.6。

**（8）复杂度对照（回应「会不会太复杂」）**

| | 原 `cmd_vel` 方案（已否决） | D 方案（已冻结） |
| --- | --- | --- |
| 新增接口 | 0 | 2 个 msg + 2 个常量 |
| `amr_power_manager` 运行时逻辑 | 订阅 + 时间窗 + 滞回 + 阈值 + mux 切换点 + 相对 topic | **1 个订阅 + 1 个发布 + 1 个查表积分** |
| 需要理解的上层语义 | 隐含（靠速度猜） | 显式（消息里写明） |
| V4 机械臂 | 盲区，需再加订阅 | 加 1 个枚举 + 1 行配置 |
| 配置项 | 3 速率 + `mode_window_sec` + `move_threshold` + 双阈值 | **3 速率 + 双阈值 + `mode_timeout_sec`** |

> **结论**：D 多两个接口定义（一次性、静态、易审查），换来运行时逻辑几乎清零。**接口多是「看得见的静态成本」，判定逻辑是「藏起来的动态成本」**——后者才是面试追问和后期维护出问题的地方。

**（9）可选最简变体（有副作用，不推荐）**

可**复用现有 `ComponentStatus` / `BEHAVIOR_STATUS`**（`amr_behavior_manager_node.cpp:52` 每 1 Hz 发布，含 `STATE_WORKING / IDLE / CHARGING`），`amr_power_manager` 直接订阅它判档，从而**连 `PowerMode.msg` 都省掉**。核查发现两点副作用，故不推荐：

1. `publishComponentStatus()`（580-589 行）把 `ACCEPTED`（等待货物批准、车静止）也算 `STATE_WORKING`，需修正判定；而该消息同时被 `amr_agent` 用于聚合对外 `state`（`amr_agent_node.cpp:87`），改动会波及 5 s 上报语义。
2. `STATE_CHARGING` 枚举目前全仓**从未被使用**（grep 无命中），等于要重新定义其触发条件。

专用 `PowerMode.msg` 语义单一、零副作用，对「实现者不一定是你」的任务书更安全。

**（10）文件级改动清单（D 方案，已冻结）**

| 动作 | 文件 | 改动 |
| --- | --- | --- |
| 新增 | `amr_interfaces/msg/PowerMode.msg`、`BatteryState.msg` | 见（2） |
| 修改 | `amr_interfaces/CMakeLists.txt` | `rosidl_generate_interfaces` 加 2 行 |
| 修改 | `amr_common/ros_interface_names.h` | 加 `POWER_MODE` / `POWER_LEVEL` 两个常量 |
| 修改 | `amr_behavior_manager_node.cpp` | 新增 `BatteryState` 订阅与缓存；`publishRobotStatus()` 里 `rs.battery = 100.0f`（562 行）改为缓存值；顺手 publish `PowerMode`；`MODE_WORK` 判定补 `NAVIGATING_HOME` |
| 修改 | `amr_agent_node.cpp` | `robot_status` 回调加 `m_battery = msg->battery`；`root["battery"] = 100.0`（296 行）改为 `m_battery`。**不新增订阅** |
| 新增 | `robot_ws/src/amr_power_manager/`（新 ROS2 包） | 上述积分逻辑 + 配置 + launch + `package.xml` / `CMakeLists.txt`（按 `AGENTS.md` C++ 规范：Tab 缩进、Allman、`m_` 前缀、`this->`、文件头注释、注册 install 目标） |
| 配置 | `amr_power_manager/config/` | `idle_drain_pct_per_min` / `work_drain_pct_per_min` / `charge_pct_per_min` / `low_soc_threshold` / `charge_target_soc` / `mode_timeout_sec` |

**（11）验收点（D 方案专属）**

- `ros2 node info /robot01/amr_power_manager`：**无** `cmd_vel` 订阅、**无** `robot_status` 订阅；只有 `power_mode` 入 / `power_level` 出。
- 给定 `PowerMode` 序列与 3 个速率可离线复算 SOC；Gazebo 暂停时 SOC 不变。
- 断开 `PowerMode` 发布 3 s，档位回落 IDLE 且日志可见。
- 双机 namespace 下各车 `power_mode` 不串台；`tbl_robot_status.battery` 两车独立变化。
- 低电量 → 自动回充 → 充到 `charge_target_soc` → 退出回充，`is_low` 无震荡。
- V4 模拟：仅加枚举值与一行配置即可让 `GraspAction` 期间按臂速率扣电，其余文件零改动。

---

#### 3.3.3 策略要求

| 策略 | 要求 |
| --- | --- |
| 低电量不接新任务 | `maybeRequestNextGoods()` 增加电量守卫；已在途的获批搬运**先完成**（与 SRS D-027「Edge 异常时完成当前已获批搬运」的保守思路一致） |
| 自动回充 | 电量低于阈值 → 申请 `CHARGE_POINT` 资源 → 导航至 `type=IDLE` 的 location → 充电 → 释放充电点 |
| 充电点占用 | 复用 `IdlePointAllocator` 的 `m_occupied`，升级为 lease（超时回收、重启恢复） |
| 抢占式中断 | BT `ReactiveFallback(BatteryOK?, GoCharge)`，在途低电量可抢占当前动作；**但不得改变对外 `task_state` 枚举** |
| 电量数据上报 | 沿用 `amr/{robot_id}/status`，不改变消息结构（若需新增字段走 TBD） |

#### 3.3.4 与 V2 其他模块的耦合点

- 回充目标点位由 `IdlePointAllocator` 给出，仍需遵守 INV-06「目的地由 `goods_id + task_type` 推导」的例外说明：充电点属于机器人运行点位，不是任务货物点位，**需在架构文档中明确这一区分**，避免破坏不变式表述。
- 充电点与路口可同时被申请，必须遵守 3.1.4 有序申请规则。
- **D 方案下电量档位与 `cmd_vel` mux 完全解耦**：电量层不订阅 `cmd_vel`，档位来自行为层 `PowerMode` 声明，因此 3.5 mux 落点（TBD-024）的后移不影响电量链路。急停时由行为层进入安全态并声明 `MODE_IDLE`（见 3.3.2.3（6）），电量随之回落 IDLE 档；**不得**为「判停稳」把 `cmd_vel` 重新接回电量模块。

#### 3.3.5 充电源形态（TBD-035 · **已冻结为 B 方案**）

项目所有者已确认：**V2 采用 B 方案**——`amr_power_manager` 本地按充电时间生成电量并累加，不引入独立充电模拟器 / 远端充电桩服务，也不实现 `ChargeSource` 接口抽象。本节保留决策记录，供实现者与面试讲述时追溯「为什么没那么做」。

| 方案 | 描述 | 结论 |
| --- | --- | --- |
| A | 新增独立「充电模拟器」HTTP 服务；`amr_power_manager` 周期性请求，每次返回一个充电值并累加；请求频率与返回值可配 | **不采用** |
| **B** | `amr_power_manager` 直接按充电时间生成电量（每分钟 `charge_pct_per_min`），本地累加 | **采用（本版冻结）** |
| C | B 作默认 + 抽象可替换 `ChargeSource` 接口，未来接 `RemoteChargeSource` | **不采用**（见下） |

**A 不采用的四个理由**（保留以便讲清取舍）：

1. **职责倒置 / 真值分裂**：由外部服务决定「充了多少」、AMR 只做加法，违背 3.0.1「SOC 单点持有」。SOC 是 BMS / 库仑计估出的本体状态，充电机只提供电压/电流，不会「告知 SOC 增量」。
2. **新引入一致性难题**：HTTP 重试 / 乱序 / 超时导致重复累加或漏加，且**无法复用** V1 的 `status/ack` + `tbl_amr_task_status_event` 幂等表（那是任务事件，不是连续量）。面试官一句「请求重试了会不会多加一次」即可问穿。
3. **破坏分层与断网语义**：INV-10 规定 AMR 内部 ROS2、AMR↔Edge HTTP/MQTT；新增独立 HTTP 服务既无明确归属，又让「充电」这一最应本地自洽的行为依赖网络，与 V1 主打的断网续行卖点冲突。同时新增部署单元，加重 `setup_simulation.sh` 可复现性问题。
4. **配置不自洽 + 不可复现**：「请求频率 × 每次返回值」相乘才是真实充电速率，两个自由度会让改频率时误改充电速度；网络抖动使电量曲线无法离线复算，V1.5 基准实验失效。

**C 不采用的理由**：B 已能跑通本版全部验收项；在没有第二个实现时，`ChargeSource` 是空接口，属于过度设计，且会让实现者误以为必须接远端服务。**V2 不保留该接口、不预留 `RemoteChargeSource` 代码**；若 V3/V4 需要模拟多机抢桩、功率降额、接触不良，直接在 `amr_power_manager` 本地注入速率变化即可（复用 V1.5 故障注入框架），不必新增部署单元。

> **B 方案的边界声明**：线性充电是**明确的仿真简化**，实现者不得自行补上 CC-CV 曲线或效率系数（见 3.3.2 约束 9）。面试陈述时应主动说明「V2 刻意采用线性可复算模型，把复杂度留给资源仲裁与行为树；真实 BMS 的 CC-CV / 库仑计融合属后续演进方向」——自己讲出边界是成熟，被问出来是减分。

---

### 3.4 行为树执行骨架（C 方案）

#### 3.4.1 分层边界（必须遵守）

```text
任务级 FSM（Edge 持久化，已冻结，V2 不改）
  PENDING / ACCEPTED / RUNNING / CANCELLED / INTERRUPTED / SUCCEEDED
        |
        | 进入单趟执行时交给 BT
        v
单趟执行 BT（V2 新增，反应式、可抢占、可组合）
  Fallback / Retry / Guard(TrafficGranted) / ReactiveFallback(BatteryOK?)
  / Navigate / Wait(Grasp 占位) / Report
        |
        +-- 对外仍按 V1 契约上报 task_state（5s）、CARRY_COMPLETED / CARRY_ABORTED、status/ack
```

**BT 内部状态不得外泄为新的 `task_state` 枚举值**。BT 节点执行到哪一步，映射回 V1 已有的 `NAVIGATING_TO_PICKUP / GRASPING / TRANSPORTING / PLACING / NAVIGATING_HOME`（状态名以 `task-state-machine.md` 与 `amr_behavior_manager_node.cpp` 实际取值为准）。

#### 3.4.2 依赖与版本（TBD-023）

| 项 | 要求 |
| --- | --- |
| BT 库 | `behaviortree_cpp_v3`（ROS2 Humble 的 Nav2 对应 3.x；**勿装 v4**） |
| 声明 | `robot_ws/src/amr_behavior_manager/package.xml` 增加 `<depend>behaviortree_cpp_v3</depend>`；`CMakeLists.txt` 同步 `find_package` 与 `ament_target_dependencies` / `target_link_libraries`（项目规则要求两者一致） |
| 可视化 | 配套 Groot 版本需与 BT.CPP 3.x 匹配（v4 的 Groot2 解析不了 3.x XML） |
| XML 存放 | 建议 `robot_ws/src/amr_behavior_manager/config/` 或 `behavior_trees/`，并注册到 CMakeLists 的 install 目标 |
| 现状 | `nav2_params.yaml:42-53` 的 `bt_navigator` 已在使用默认 `navigate_to_pose_w_replanning_and_recovery.xml`，V2 可复用其范式但**不修改 Nav2 自带 XML** |

#### 3.4.3 节点清单（建议，具体注册方式以 TBD-023 为准）

| 节点 | 类型 | 职责 | 对应 V1 现状 |
| --- | --- | --- | --- |
| `CarryRoot` | Fallback / ReactiveFallback | 单趟执行总入口，异常统一收口 | 散落的 if-else 分支 |
| `Retry(Navigate, N)` | 装饰节点 | 导航失败重试 N 次后走 Abort | D-047「不自动重试，直接 CARRY_ABORTED」 |
| `Guard(TrafficGranted, Navigate, Wait)` | 条件 + 分支 | **闸门语义**：未获路权时 `cmd_vel` 归零等待、不放行该段；授权已到则**零动作**继续原 `NavigateToPose(D)`。对应 3.2.2 | V2 新增 |
| `RequestOnApproach` | 动作节点（替代原 `PreRequestNextRight` + `LookAheadPath`） | 只读 `amr_navigate_to_pose` action feedback 的 `current_pose`（**不订阅 `plan`**）；中心点 `inside` 进区边沿即发 `TrafficRequest(request_id, ETA, lease_ttl)`，边沿时刻算 `entry_side` / 就近等待点并锁存；`REQUESTING` 收 `GRANTED` **零动作**。**不调用 `ComputePath`、不缓存路径、无 `onPlan()` / `D_trigger` 预判** | V2 新增 · TBD-038 |
| `WaitAndRetry` | 动作节点 | 被拒后 cancel 当前 goal → **等 result 回调 `m_goal_active=false`** → 发 `NavigateToPose(同侧就近等待点)` → 固定 `T_retry = 10 s` 用同一 `request_id` 幂等重发；**仅 `WAITING` 收 `GRANTED` 才**立即 `navigateToPose(D)`，不等轮询 | V2 新增 |
| `ReleaseTrafficRight` | 动作节点 | **出区边沿 / 任务结束 / `lease_ttl` 到期 / 显式 `CANCEL`** 四触发取最早即释放，并显式 `cancel` lease；**与导航 Action 结束解耦**（INV-13）。改道驶离由出区边沿、停住 / 断流由 TTL 承接，**无独立 `plan` 改道检测机制** | V2 新增 |
| `ReactiveFallback(BatteryOK?, GoCharge)` | 反应式回退 | 低电量抢占；`BatteryOK` 数据源为 `amr_power_manager` 发布的 `BatteryState.is_low`（D 方案，见 3.3.2.3（5）），节点本身不判定阈值 | V2 新增 |
| `Guard(GoodsApproved, ...)` | 条件 | 货物申请获批才进入搬运 | `handleGoodsResponse` |
| `Wait(Grasp)` | **占位叶节点** | V1 的固定延时抓取，V4 替换为 `GraspAction` | D-045 `grasp_simulate_seconds` |
| `Wait(Place)` | **占位叶节点** | V1 的固定延时放置 | D-045 `place_simulate_seconds` |
| `NavigateToPose` | Action 节点 | 调用 `amr_navigate_to_pose` | `m_nav_client->navigateToPose(goal)` |
| `Report` | Action 节点 | 发 `CARRY_COMPLETED` / `CARRY_ABORTED` | `publishTaskEvent` |
| `AbortAndReport` | Fallback 末端 | 上报 `CARRY_ABORTED` + 释放资源 | `onNavigationResult(false)` |

> **Grasp 占位是 V4 不返工的关键**：V2 树里 `Grasp` 是一个叶节点接口；V4 只替换该节点内部实现（接 MoveIt / 夹爪 / 接触检测），**父树、Guard、Fallback、任务级 FSM 全部不动**。

#### 3.4.4 示意结构（伪 XML，仅表达层级，非可直接编译配置）

```xml
<root main_tree_to_execute="CarryTree">
  <BehaviorTree ID="CarryTree">
    <ReactiveFallback name="carry_root">
      <Condition ID="BatteryOK"/>
      <Fallback name="carry_body">
        <Sequence name="do_carry">
          <Condition ID="GoodsApproved"/>
          <Condition ID="TrafficGranted"/>
          <RetryNode name="nav_retry" num_attempts="3">
            <Action ID="NavigateToPose"/>
          </RetryNode>
          <Action ID="Grasp"/>          <!-- V2: Wait 占位；V4: GraspAction -->
          <Action ID="NavigateToPose"/>
          <Action ID="Place"/>          <!-- V2: Wait 占位；V4: PlaceAction -->
          <Action ID="Report"/>
        </Sequence>
        <Action ID="AbortAndReport"/>
      </Fallback>
    </ReactiveFallback>
  </BehaviorTree>
</root>
```

> 具体节点标签、属性名、黑板（Blackboard）字段、自定义节点注册方式必须由 TBD-023 按实际 BT.CPP 3.x API 确认后编写，**不得直接照抄本示意**。
>
> **按需触发模型下的读法**：示意中的 `<Condition ID="TrafficGranted"/>` 是闸门条件，实际由 `RequestOnApproach` 在**行驶途中按中心点进区边沿**驱动（提前量由 `L_pull` 拉长控制区自带，不靠 `plan` 预判），不是到点才检查；授权提前到达时该条件直接通过、不重发 goal（3.2.2 ★ 分支，`REQUESTING` 零动作）。示意树未画出 `WaitAndRetry` 与 `ReleaseTrafficRight` 的进出区边沿回调，实现时须补。

#### 3.4.5 与 `amr_agent` 的接口

`amr_agent` 与 `amr_behavior_manager` 继续通过 ROS2 Topic `task_command` / `task_event` 通信（常量在 `amr_common/ros_interface_names.h`）。**V2 不新增、不修改这两个消息结构**；BT 完全位于 `amr_behavior_manager` 内部。若确需新增字段，按重大变更流程确认。

**路权消息不走这两条 topic**：V2 新增一对**已定名**专用 ROS2 topic —— `traffic_request`（`amr_common::topic::TRAFFIC_REQUEST`，`amr_behavior_manager` → `amr_agent`）与 `traffic_response`（`amr_common::topic::TRAFFIC_RESPONSE`，`amr_agent` → `amr_behavior_manager`），消息为 `amr_interfaces/msg/TrafficRequest.msg` / `TrafficResponse.msg`，由 `amr_agent` 内补实现的 `TrafficBridge` 与 MQTT 互转；范式照抄 V1 已有 `TaskBridge` 的 `registerExternalHandler()` + `setExternalSubscribe()`。**命名与字段已由 INV-14 冻结，不得自行更改**。逐跳接口与消息字段见 [V2 交通路权控制链 §4 / §7](v2-traffic-control-chain.md)、[V2 实现规格 §1 / §2](v2-implementation-spec.md)。

#### 3.4.6 Groot 可视化与验证联动

- 运行时启用 BT 日志/状态发布，Groot 中能实时看到当前 tick 节点高亮。
- 与 V1.5 故障注入框架联动：注入一次导航失败 → 画面上 `Retry` 节点亮 3 次 → `Fallback` → `AbortAndReport` → 状态上报 `CARRY_ABORTED`。该场景同时作为 V2 的验证证据与演示片段。

---

### 3.5 软件急停与 `cmd_vel` 优先级仲裁（TBD-024）

#### 3.5.1 必须实现的优先级

```text
ESTOP  >  交通等待(Guard 未通过)  >  Nav2  >  手动调试指令
```

高优先级可覆盖低优先级；ESTOP 生效时所有下游 `cmd_vel` 必须为 0，且**不可被 Nav2 的重规划自动恢复**。

> **作用域注（模型 A 定案）**：「交通等待」闸门覆盖的是「上一段导航结束后的残余速度」与「防止在授权未到时误发新 goal」，**不是压住一个仍在执行的 Nav2 goal**。等待期间 Nav2 无活动 goal（3.2.3），因此不会触发 `Failed to make progress`。各窄道闸门由**独立状态机按运行时实际遇到的顺序**驱动，不预排序列；同一时刻不得并发放行两个冲突区（Edge 中心仲裁保证，3.2.5）。

#### 3.5.2 要求

- 触发源（HTTP / MQTT / ROS2 topic）与仲裁落点（新节点 or `navigation_server` 内）由 TBD-024 确认。
- 急停后进入明确状态，恢复需**显式复位**（建议人工复位，避免自动恢复带来安全风险）。
- 急停期间：任务保持当前 `task_state`，不伪造完成、不丢失待补报（INV-03/09）。
- 与 V4 的关系：**安全互锁在 V2 先建好**，V4 车顶臂只是复用（臂运动前底盘锁止、急停双切），不在 V4 才补。

---

### 3.6 降级与恢复策略矩阵（必须产出文档 + 实现）

| 故障场景 | 系统行为 | 恢复条件 | 上报 |
| --- | --- | --- | --- |
| 路权申请超时 | BT 等待分支，退避重试 | 获批或达上限 | 达上限 → `CARRY_ABORTED` |
| 持权 AMR 掉线 | 自动回收租约 | 租约过期 / 心跳超时 | 记录事件，不广播给其他 AMR |
| Edge 重启 | 从 SQLite 重建 lease 并向在线 AMR 重发授权 | 启动完成 | 不改变任务终态 |
| 充电桩占满 | 排队等待，占用释放后按序分配 | 有空闲充电点 | 状态快照更新 |
| 低电量发生在在途 | 完成当前已获批搬运后回充 | 电量恢复阈值 | `task_state` 按 V1 枚举上报 |
| 导航失败 | BT Retry → Fallback → 释放路权与配额 | 重试成功或放弃 | `CARRY_ABORTED` / `NAVIGATION_FAILED` |
| 授权提前到达（正常路径，S1） | `REQUESTING` 收 `GRANTED` **零动作**：不减速、不重发 goal、不切分任务 | — | 无事件，日志记录「提前授权」用于取证 |
| 申请被拒 / 超时 | cancel 当前 Nav2 goal → **等 result 回调 `m_goal_active=false`** → `NavigateToPose(同侧就近等待点)` → `WAITING`，固定 10 s 幂等重发 | 仅 `WAITING` 收 `GRANTED` 立即重发 `NavigateToPose(D)` | 记录停顿原因（申请未回），不改 `task_state` |
| 等待期间 Nav2 无活动 goal | 本就**无 `plan` 订阅**、不做路径预判、不调 `ComputePath`；「是否绕行」留到重发 D 之后才知道 | 收到 `GRANTED` 立即重发 D | 记录「等待态无活动 goal」 |
| 重发 D 后 Nav2 改道绕行 | **改道即释放保留语义、移除独立机制**：驶离控制区 → 「出区边沿」释放；停住 / 断流 → `lease_ttl` 到期 `EXPIRED`；无 `plan` 相交 / 位姿远离 / 去抖实现 | 权限回收，对向车可申请 | 记录「出区边沿释放」或 `EXPIRED` 事件 |
| 中心点出区边沿漏检 | 导航 `SUCCEEDED` / 任务结束 / 异常 `ABORTED` 兜底释放 | 兜底触发 | 记录「兜底释放」及漏检原因，供取证 |
| `lease_ttl` 到期 / AMR 崩溃 / `cancel` 丢包 | Edge 自动回收 lease；AMR 若仍需使用须重新申请，不得静默续租 | 重新获批 | 记录 `EXPIRED` 事件，不改 `task_state` |
| AMR 显式取消申请 | 排队中 → 出队；已授权 → 释放 lease + 配额回退 | 取消被确认 | 记录 `CANCELLED`，带 `epoch` 丢弃迟到消息 |
| 排队中请求长期未获批 | Edge 按 `PENDING` 超时作废（TBD-020），AMR 侧重发会生成新排队项 | 重新入队或获批 | 记录出队原因，不广播给其他 AMR |
| 急停 | `cmd_vel` 归零，任务挂起 | 显式复位 | 保持当前状态 |
| MQTT 断连 | 沿用 V1：完成当前趟、停止下一趟、待补报落盘 | 连接恢复 + ACK | 沿用 V1 契约 |
| 充电前置条件破坏（离开充电点 / 停稳失效 / 接触中断 / lease 丢失） | 行为层撤销 `MODE_CHARGING` 并改声明 `MODE_IDLE`/`MODE_WORK`，电量层停止充电积分、按新档位转为放电；释放 `CHARGE_POINT` lease | 重新满足 3.3.2 充电前置条件 | `task_state` 按 V1 枚举，必要时记录事件 |
| `PowerMode` 消息丢失 / 行为层挂掉 | 保持上一档位；超过 `mode_timeout_sec`（建议 3 s）未收到则回落 `MODE_IDLE`，按最低速率扣电并记日志，不报错、不中断 | `PowerMode` 恢复 | 沿用 V1 状态上报 |
| 充电中 lease 超时 / 被抢占 / 急停 | 行为层撤销 `MODE_CHARGING`、电量层立即停止积分、释放 `CHARGE_POINT` lease；急停另由 3.5 mux 归零 `cmd_vel`（运动控制，与档位判定无关） | 显式复位或重新获批 | 保持当前 `task_state`，不伪造完成 |
| 电量积分时间基准异常（`/clock` 跳变 / Gazebo 暂停） | SOC 不跳变；暂停期间不积分；恢复后按 `dt_sim` 累计 | 仿真时钟恢复 | 沿用 V1 状态上报 |

---

## 4. 接口与数据模型变更汇总

> 以下均为**规划**，实施前须按 [`../version-roadmap.md`](../version-roadmap.md) 第 0.2 条第 5 款（对应 `AGENTS.md` 的重大变更规则）确认，并同步 SRS / 接口契约 / 架构文档。

| 类别 | 变更 | 位置 | 状态 |
| --- | --- | --- | --- |
| MQTT Topic | **已定名**：`amr/{robot_id}/traffic_rights/request`（上行，payload `op`）、`amr/{robot_id}/traffic_rights/response`（下行单播）；Edge 订阅 `amr/+/traffic_rights/request` | `common/mqtt/mqtt_topic.h`：替换 `TRAFFIC_RIGHTS_REQ` / `TRAFFIC_RIGHTS_RSP_PREFIX` 为 `AMR_TRAFFIC_REQUEST` / `AMR_TRAFFIC_RESPONSE` / `AMR_TRAFFIC_SUBSCRIBE` | **已冻结（INV-14）** |
| MQTT 消息 | `TrafficRequest` / `TrafficResponse` 的 JSON 编码（字段见实现规格 §2）；`op` = `REQUEST` / `CANCEL` / `RELEASE`；充电点申请/响应（若复用统一仲裁则同一结构） | `interface-contract.md` 第 8 节（V2 路权接口） | **字段与编码已定案**，表结构与数值参数待 TBD-020 |
| 数据表 | `tbl_resource_lease`（建议，TBD-020） | `edge_server/src/database/sqlite_db.cpp` | 待定 |
| 数据表 | 电量日志表（可选，TBD-022） | 同上 | 待定 |
| ROS2 接口 | **AMR 内部**电量 topic（D 方案，已冻结）：`power_mode`（`amr_behavior_manager` → `amr_power_manager`）、`power_level`（`amr_power_manager` → `amr_behavior_manager`），相对 topic，每机器人 namespace；现有跨包接口不变 | `amr_common/ros_interface_names.h` 登记 `POWER_MODE` / `POWER_LEVEL` | **已冻结（D）** |
| ROS2 消息 | `PowerMode.msg`、`BatteryState.msg`（专用、单向、不含 `battery`）；`amr_power_manager` **不订阅** `cmd_vel` / `robot_status` / `task_status` / `task_state` | `amr_interfaces/msg/`、`amr_interfaces/CMakeLists.txt` | **已冻结（D）** |
| 配置 | 独立 `traffic_gates.yaml`（`resource_id` + 4 点凸 `polygon` 通行控制区 + `wait_poses{l1,l2}`，**无 `NARROW` / `TRAFFIC_WAIT` / `side` / `trigger` / `release`**，TBD-021）；三个速率 `idle_drain_pct_per_min` / `work_drain_pct_per_min` / `charge_pct_per_min`、`mode_timeout_sec`、双阈值、仿真时间基准（速率数值待 TBD-022；形态已由 TBD-035=B / TBD-036=D 冻结）；BT XML 路径与重试次数 | 新增 `data/warehouse_01/traffic_gates.yaml`、`robot_ws/src/amr_power_manager/config/`、`amr_behavior_manager/config/` | 形态已定案、数值待定 |
| 地图数据模型 | **独立 `traffic_gates.yaml`** 承载 4 点 `polygon` 通行控制区（端边外移 `min_release_clearance` / 侧边拉长 `L_pull`）+ `wait_poses{l1,l2}`；V1 `zones.yaml` / `stations.yaml` / `ZoneInfo` / `Location` **一律不动**、不扩展 type 菜单；`MapInfo.msg` 加 `traffic_gates_yaml`，打包下发 8 处链路；**不新增「路线 / 顺序」表、不新增途经点**；`metadata.yaml` 版本升级（TBD-003） | 新增 `data/warehouse_01/traffic_gates.yaml`、`tool/tool_ws/.../traffic_gates_editor`、`amr_behavior_manager/traffic_gate_loader.h` | 几何形态已定案（字段命名 / `L_pull` 数值 TBD-021 / 038） |
| 地图校验 | `map_tool/validator.py` **新增 7 条内容校验**（4 点凸 / 点序 / 端边 `L_pull` / 与 walls 不冲突 / 等待点分组且端边外侧 / 不与分叉点前通道重叠 / `resource_id` 唯一）；原实现仅检查文件存在 | `tool/map_tool/validator.py` | 已定案（规则数值 TBD-021 / 038） |
| 导航接口 | **已定案不引入**：`ComputePath` / `FollowPath` 独立 Action Client、`NavigateThroughPoses` 均排除；目标全程 `NavigateToPose(D)`，**提前量由 `L_pull` 拉长控制区自带、不订阅 `plan`**，位姿取 action feedback `current_pose`（中心点）。**不新增 ROS2 导航接口** | `amr_navigation/` 仅修 4 个缺陷，不改对外接口 | 已冻结 |
| 依赖 | `behaviortree_cpp_v3` | `package.xml` + `CMakeLists.txt` | 待定 |
| 代码 | 移除两处 `battery = 100.0` 硬编码，改由 `amr_power_manager` 提供数值 | `amr_agent_node.cpp:296`、`amr_behavior_manager_node.cpp:562` | 明确 |
| ROS2 接口 | **跨包接口不变**（`task_command` / `task_event` / `amr_navigate_to_pose`） | `amr_common/ros_interface_names.h` | 冻结 |

---

## 5. 文件级改动清单

| 类型 | 路径 | 说明 |
| --- | --- | --- |
| 新增 | `edge_server/src/service/resource_broker.*`（建议名，TBD-020） | 统一仲裁：授权、租约、回收、重启恢复、有序申请 |
| 新增 | `edge_server/src/service/traffic_rights_service.*`（建议名） | 路权申请 / **FIFO 排队** / 显式取消 / 响应 / `lease_ttl` 超时回收；通配符订阅按动作分层（3.2.5） |
| 新增 | `robot_ws/src/amr_power_manager/`（**新 ROS2 包，归属更正**） | 电量真值 SOC、B 方案速率积分（IDLE/WORK 两档消耗 + 充电）、**D 方案**：订阅 `PowerMode` 查表积分（零判定、零 `cmd_vel` 订阅）、`mode_timeout_sec` 回落、电池配置、充电状态机；见 3.0.1 / 3.3.2 / 3.3.2.3 / 3.3.5 |
| 新增 | AMR 内部电量 ROS2 接口（**TBD-036 已冻结为 D**）：`PowerMode.msg` / `BatteryState.msg` + `POWER_MODE` / `POWER_LEVEL` 常量 | `power_mode` 由行为层声明档位、`power_level` 输出 SOC 与 `is_low`；battery 真值经 `robot_status` 分发给 `amr_agent`，`is_low` 供 BT `BatteryOK` 消费；**不给 `amr_agent` 加订阅**；需在接口契约登记，见 3.3.2.3（2） |
| 新增 | `amr_behavior_manager` 的 `PowerMode` 发布 | 复用已有 1 Hz `publishRobotStatus()` 定时器，同函数内多 publish 一条 `PowerMode`，零新增定时器；见 3.3.2.3（3） |
| 新增 | `robot_ws/src/amr_behavior_manager/behavior_trees/*.xml` | BT 定义（TBD-023 确认路径） |
| 新增 | `robot_ws/src/amr_behavior_manager/src/bt_nodes/*.cpp` | 自定义节点：BatteryOK / TrafficGranted / GoodsApproved / Grasp 占位 / Report 等 |
| 新增 | `robot_ws/src/amr_navigation/` 或新包内急停仲裁节点 | `cmd_vel` mux（TBD-024） |
| 修改 | `edge_server/src/database/sqlite_db.cpp` / `.h` | 新增 lease 表、查询与事务 |
| 修改 | `edge_server/src/communication/amr_communication.cpp:111` | 启用路权订阅，改为 `amr/+/traffic_rights/request`（只订上行），按 payload `op` 分发；`robot_id` 与 topic 交叉校验（3.2.5） |
| 修改 | `edge_server/src/service/idle_point_allocator.*` | 充电点分配升级为 lease |
| 修改 | `robot_ws/src/amr_agent/src/amr_agent_node.cpp:296` | 移除 battery 硬编码；在已有 `robot_status` 回调里缓存 `msg->battery`（由 behavior manager 填入 D 方案真值），**不新增订阅**；上报消息结构不变，见 3.3.2.3（10） |
| 修改 | `robot_ws/src/amr_behavior_manager/src/amr_behavior_manager_node.cpp` | 单趟执行段迁移到 BT；移除 562 行 `battery=100.0f` 硬编码、改为 `BatteryState` 缓存值；订阅 `BatteryState`；在 `publishRobotStatus()` 内发布 `PowerMode` 并补 `NAVIGATING_HOME` 判 WORK；保留对外状态上报，见 3.3.2.3（3）（10） |
| 修改 | `robot_ws/src/amr_behavior_manager/package.xml`、`CMakeLists.txt` | 增加 `behaviortree_cpp_v3` 依赖与 install 目标 |
| 新增 | `data/warehouse_01/traffic_gates.yaml` | 独立文件：`resource_id` + 4 点凸 `polygon` 通行控制区 + `wait_poses{l1,l2}`；**V1 `zones.yaml` / `stations.yaml` 一律不动**、不新增 `NARROW` / `TRAFFIC_WAIT` / `side`；`metadata.yaml` 加第 4 文件并升版本；**不新增路线 / 顺序表**，见 3.2.1.3 |
| 新增 | `tool/tool_ws/src/warehouse_tool/traffic_gates_editor.*`（新节点 + 新 manager + `CMakeLists.txt` + 第 4 个菜单项） | 两阶段交互（4 点 zone + N 等待点）；复用 `ClickedPointReceiver` / `ZoneMarkerPublisher`，**不复用 `zone_editor` 节点本体 / `YamlZoneManager` / `ZoneInfo` / `selectZoneType()`**；新建 `TrafficGate` 数据结构，见 3.2.1.4 |
| 修改 | `tool/map_tool/validator.py` | **新增 7 条内容校验**规则（原仅检查文件存在），见 3.2.1.4 |
| 新增 | `robot_ws/src/amr_behavior_manager/src/traffic_gate_loader.cpp`、`include/.../traffic_gate_loader.h` | 读取独立 `traffic_gates.yaml` 的 4 点 `polygon` + `wait_poses`，供中心点 `point_in_poly` 进出边沿、`entry_side` 计算与等待点导航（TBD-039）；`MapInfo.traffic_gates_yaml` 下发文件路径（`YAML::LoadFile`）；**V1 `zone_station_loader` 不改**，见 3.2.1.1 |
| 修改 | `robot_ws/src/amr_navigation/src/navigation_server.cpp`、`navigation_manager.cpp` | **修 4 个阻塞缺陷**：supersede、60 s 超时成对 cancel、`m_nav_epoch` 竞态/无锁状态、`accepted`→`submitted`，见 3.2.3 / 3.7 第 4 步 |
| 修改 | `robot_ws/src/amr_navigation/config/nav2_params.yaml` | 仅在等待策略影响进度检查器时调整，需附调参记录与快照 |
| 新增 | `docs/architecture/adr/0002-resource-arbitration-and-bt-layering.md` | 记录统一仲裁与 BT 分层的决策、动机、取舍 |
| 新增 | `docs/architecture/roadmap/` 下本分册的验证记录 | Groot 截图、故障注入结果 |
| **禁止改动** | 任务级 `state` 状态机、`task_state` 枚举、`status/ack` 与待补报逻辑、`goods_status` 四量、`amr_common` 现有常量语义 | 冻结 |

---

## 6. 文档同步清单

| 文档 | 动作 |
| --- | --- |
| `docs/requirements/srs.md` 第 8.1/8.2 节 | 交通与电量由 Planned → 本版实现范围；**需项目所有者授权后修改** |
| `docs/requirements/interface-contract.md` 第 4/7 节 | 新增路权/资源申请、**显式取消**、释放消息与 QoS、幂等字段（`request_id` / `epoch` / `lease_ttl`）、通配符订阅与动作分层约定；更新 TBD |
| `docs/architecture/edge-data-model.md` | 新增 `tbl_resource_lease` 与电量表结构 |
| `docs/architecture/edge_server.md` | 新增 ResourceBroker / Traffic 服务；明确 **不含** 电量计算（归属见 3.0.1） |
| `docs/architecture/amr.md` / `system.md` | 明确 **V2 路权逻辑内嵌于 `amr_behavior_manager`（`TrafficGate`），不设独立模块**，删除两处 mermaid 中的独立 traffic service；`amr_power_manager` 由 V2/Planned → 本版实现；补充 B 方案速率模型、**D 方案 `PowerMode` 单向声明链路**、SOC 真值边界 |
| `docs/architecture/amr-application-design.md` | 新增 BT 分层说明与节点清单 |
| `docs/architecture/task-state-machine.md` | 增补「BT 内部状态 ≠ 对外 task_state」的边界说明（不改枚举） |
| `docs/architecture/system.md` | 更新 V2 模块图 |
| `docs/architecture/adr/README.md` | 新增 ADR-0002 索引 |
| `AGENTS.md` 第 5 节 | 记录本版新增的已冻结决策（实施后） |
| `README.md` / `docs/assets/architecture.svg` | 更新版本范围与架构图 |

---

## 7. 实施顺序建议

1. **先冻结设计**：关闭 TBD-020 ~ TBD-024、TBD-038、TBD-039（TBD-035 已冻结为 B、TBD-036 已冻结为 D；**运行时按需触发 / 目标全程 D / 中心点进出边沿申请释放 / 固定 10 s / Edge 中心排队 + TTL 兜底** 为第五轮定案；**模块归属 / ROS2+MQTT topic 命名 / payload `op` / 消息字段 / V2 需求写入 SRS** 为第七轮定案，见 INV-14 与 [V2 实现规格](v2-implementation-spec.md)；**4 点通行控制区 / 中心点主判据 / 无 `plan` 订阅 / 独立 `traffic_gates.yaml`** 为 2026-11-22 V0.20 八项拍板，见 `.workbuddy/artifacts/traffic-gate-8-decisions-frozen.md`），产出 ADR-0002，按重大变更流程征得同意。开工前先读实现规格 §0〈定案速查〉。
2. **数据层先行**：建 lease 表 + 原子授权 + 超时回收 + 重启恢复，先用单元测试/脚本验证幂等。
3. **地图几何与工具链**：按 3.2.1 新建独立 `traffic_gates.yaml`（4 点凸 `polygon` 通行控制区 + `wait_poses{l1,l2}`）、`traffic_gates_editor` 两阶段交互、`map_tool` 7 条校验、`MapInfo.traffic_gates_yaml` 打包下发 8 处链路；新建 AMR 侧 `traffic_gate_loader` 读取 `polygon` / `wait_poses`（V1 `zones.yaml` / `stations.yaml` / loader / 菜单一律不动）。**先用脚本验证「同一份数据正反向均由中心点 `inside` 进出边沿触发、无需第二份数据；进入侧 `s` 符号判 L1/L2 且锁存；等待点在入口端边外侧（上游）；端边不压分叉点」**，再进下一步。
4. **修 4 个导航缺陷（阻塞项）**：无条件 supersede、60 s 超时未成对 cancel Nav2 goal、`m_nav_epoch` 竞态与无锁共享状态、`accepted`→`submitted`。多路口下会密集触发，不修则后续无法验证（评审意见稿 §4 / §10.6）。
5. **接入路权**：启用通配符订阅与 Edge FIFO 排队，跑通「两车申请同一路口只有一个获批」「显式 cancel 被正确感知」「`lease_ttl` 到期回收」；填实 `amr_behavior_manager_node.cpp:125-129` 的 feedback 空回调，接中心点 `inside` 进出边沿 + 滞回去抖 + `entry_side` 锁存 + 同侧就近等待点重导航 + 固定 10 s 重发，验证「**授权提前到达时零 goal 重发、零停顿（S1 零动作 / S2 立即重发 D）**」与「重发 D 后绕行由出区边沿或 TTL 及时释放、无 `plan` 订阅」。
6. **接入电量**：先在 AMR 侧建 `amr_power_manager`（归属见 3.0.1），按 **D 方案（TBD-036 已冻结）** 落地：新增 `PowerMode.msg` / `BatteryState.msg` 与两个常量，行为层在已有 1 Hz 定时器内发布 `PowerMode`，电量层只订阅 `PowerMode` + 查表积分（`dt_sim` 仿真时钟、clamp、双阈值、`mode_timeout_sec` 回落），**不订阅 `cmd_vel`、不需要 mux 就绪、无订阅点切换**。移除两处硬编码，接入 `IdlePointAllocator` lease，跑通低电量回充。**先验证给定 `PowerMode` 序列可离线复算 SOC 曲线**，再接 Edge 侧只读镜像。见 3.3.2.3。
7. **迁移 BT**：加依赖、写自定义节点与 XML，把单趟执行段从 if-else 迁移过来，**逐条核对对外 `task_state` 未变**。
8. **急停与仲裁**：接入 `cmd_vel` mux 并验证优先级。
9. **验证与证据**：用 V1.5 故障注入框架跑 3.6 矩阵，Groot 录制 Retry/Fallback 演示片段；N 路口连续通过与反向通行各录一段。
10. **文档同步与 DoD**：按第 6 节与路线图第 5 节收尾。

---

## 8. 验收清单

- [ ] 两车同时申请同一路口，仅一台获批，另一台等待且 `cmd_vel` 为 0；无碰撞。
- [ ] 持权 AMR 掉线后租约被自动回收，路口可被重新申请。
- [ ] Edge 重启后占用状态正确重建，不出现资源永久锁定或重复授权。
- [ ] `battery` 不再恒为 100.0；`tbl_robot_status.battery` 随运行下降、回充上升；**Edge 侧不存在任何累加/修改 SOC 的代码路径**（3.0.1 红线）。
- [ ] 低电量不申请新货物、完成当前趟后自动回充并释放充电点；`low_soc_threshold` 与 `charge_target_soc` 分离，阈值附近无震荡。
- [ ] 电量积分使用 `use_sim_time` 仿真时钟；Gazebo 暂停时 SOC 不变、加速时按仿真时间正确推进。
- [ ] 充电前置条件（到位 / 停稳 / 持有 lease / 接触确认，由行为层 / 仲裁层保证）任一不满足即由行为层改声明 `MODE_IDLE`/`MODE_WORK`，电量层停止充电积分；中断后正确释放 `CHARGE_POINT` lease。
- [ ] 消耗按行为层声明的 `MODE_IDLE` / `MODE_WORK` 两档生效；`amr_power_manager` **零判定逻辑**，档位唯一来源是 `PowerMode`。
- [ ] `power_mode` / `power_level` 均使用相对 topic；双机 namespace 下各车档位与 SOC 不串台，`tbl_robot_status.battery` 两车独立变化。
- [ ] `amr_power_manager` **未订阅** `cmd_vel` / `robot_status` / `task_status` / `task_state`（源码与 `ros2 node info` 均可核对，只有 `power_mode` 入、`power_level` 出）。
- [ ] `PowerMode` 丢失超过 `mode_timeout_sec` 回落 `MODE_IDLE` 且日志可见；`NAVIGATING_HOME` 期间按 WORK 档扣电（不漏算返航）。
- [ ] 急停后行为层进入安全态并声明 `MODE_IDLE`，电量回落 IDLE 档，不按 WORK 扣电（不依赖电量层读 `cmd_vel`）。
- [ ] 充电为 B 方案本地速率积分：无独立充电模拟器进程、无 HTTP 充电请求、无 `ChargeSource` 抽象残留。
- [ ] 给定 `PowerMode` 序列与三个速率配置可离线复算 SOC 曲线；SOC 始终 clamp 在 `[0,100]`；V1.5 基准实验能输出确定性的电量变化表；Gazebo 暂停时 SOC 不变。
- [ ] 导航失败时 BT `Retry` 生效，达到上限后 `Fallback` 上报 `CARRY_ABORTED` 并释放路权/配额。
- [ ] **同一份独立 `traffic_gates.yaml`，正向 A→D 与反向 D→A 均由 AMR 中心点 `inside` 进 / 出区边沿触发**；方向无关、掉头从入口退出天然触发「出区」；无人工录入的路线顺序表、无预求全序列、反向无需第二份数据；端边不压分叉点 → 走绕行路的车不进区、零申请、零停顿（S6）。
- [ ] `NavigateToPose` 目标全程为 D：`REQUESTING` 收 `GRANTED` **零动作（零 goal 重发、零停顿）**、仅 `WAITING` 收 `GRANTED` 才立即重发 D；仅被拒时才 cancel + **等 result 回调 `m_goal_active=false`** + 到同侧就近等待点 + 重发 D；`task_state` 全程不变、无 `INTERRUPTED`、无重复 `CARRY_*` 上报。
- [ ] 等待点位于**入口端边外侧（上游）**、每侧 ≥ 1 个、组内就近且 `entry_side` 本次穿越周期锁存；仅在授权确实未到时停车，日志可区分停顿原因（申请未回 / 急停）；等待期间 Nav2 无活动 goal、**全程无 `plan` 订阅**，`ros2 node info` 与日志均可核对。
- [ ] 固定 `T_retry = 10 s`（**无 jitter**）幂等重发沿用同一 `request_id`；仅 `WAITING` 收到 `GRANTED` 立即重发 D，不等轮询。
- [ ] 4 点凸 `polygon` 通行控制区：端边外移 `min_release_clearance`、侧边拉长 `L_pull`、端边不压分叉点；**中心点 `inside` 出区边沿即释放**（主判据，含掉头退出 / 改道驶离），`SUCCEEDED` 已降级为兜底；边沿检测 + 滞回双阈值生效（TBD-038）；释放与导航 Action 结束解耦（INV-13）。
- [ ] 重发 D 后 Nav2 改道绕行 → 车驶离控制区由**出区边沿**释放、停住 / 位姿断流由 `lease_ttl` 回收（「改道即释放」保留语义、**无独立 `plan` 改道检测机制**），权限不被占死、对向车不白等。
- [ ] 申请带 `lease_ttl`；`lease_ttl` 到期 / AMR 崩溃 / `cancel` 丢包时 Edge 自动回收，无幽灵占用。
- [ ] AMR 显式 `cancel` 被 Edge 正确区分「排队中出队」与「已授权后释放」；带旧 `epoch` 的迟到消息被丢弃。
- [ ] Edge 通配符订阅按动作可辨（分层 topic 或 payload `op`），`robot_id` 与 topic 交叉校验；两车 FIFO 排队不串台、占用状态不广播。
- [ ] `NavigateThroughPoses`、**`ComputePath` / `FollowPath` 独立 Action Client 均未被引入**（源码与 `nav2_params.yaml` 使用记录均可核对）；**`plan` 订阅 / `onPlan()` / `onPlanChanged()` / `pathIntersects*` / `D_trigger` / 改道去抖变量全部不存在**，提前量由 `L_pull` 拉长控制区自带；`traffic_gates.yaml` 与 `MapInfo.traffic_gates_yaml` 打包下发 8 处链路齐全、V1 `zones.yaml` / `stations.yaml` / type 菜单零改动。
- [ ] Groot 可实时显示当前 tick 节点；故障注入触发 Retry/Fallback 的画面已归档。
- [ ] 急停生效时所有下游 `cmd_vel` 为 0，且不可被 Nav2 自动覆盖；显式复位后恢复。
- [ ] 第 3 节全部跨版本不变式逐条复核未破坏；`task_state` 枚举与 V1 完全一致。
- [ ] `colcon build --packages-select amr_behavior_manager` 通过；`package.xml` 与 `CMakeLists.txt` 依赖一致。
- [ ] 降级矩阵 3.6 每个场景有实测记录。
- [ ] 第 6 节文档同步完成，状态词与代码一致；README 与架构图更新。
- [ ] 提交说明附「为什么这么实现」（统一仲裁的抽象动机、BT 分层边界、急停优先级取舍）。

---

## 9. 本版本 TBD

| 编号 | 问题 | 需要明确的具体内容 | 开工前必须明确 |
| --- | --- | --- | --- |
| TBD-020 | 资源仲裁统一接口与 Edge 排队 | **已定案**：中心排队在 Edge、FIFO、显式取消一等事件、TTL 回收、`request_id` 幂等 + `epoch` 防陈旧、通配符订阅且动作可辨、复杂度边界（不做抢占/联合预约/死锁检测/广播）；**第七轮追加**：模块归属（INV-14）、ROS2 topic `traffic_request` / `traffic_response`、MQTT topic 带 `robot_id`、动作由 payload `op` 承载（不分层）、消息字段集合与 JSON 编码。**待明确**：是否新增 `tbl_resource_lease`；`queue_seq` 生成方式、`PENDING` 超时作废时长、`lease_ttl` 默认值与续租规则、表最终字段；与 `tbl_task_goods_allocation` 的一致性策略。见 3.1.2 / 3.2.5、实现规格 §1 | 是 |
| TBD-021 | 窄道建模与控制区几何（`L_pull`） | **已定案**：独立 `traffic_gates.yaml`（`resource_id` + 4 点凸 `polygon` 通行控制区 + `wait_poses{l1,l2}`，**无 `NARROW` / `TRAFFIC_WAIT` / `side` / `trigger` / `release`**）；点序 `P1→P2` 端边 L1 / `P2→P3` 长轴 / `P3→P4` 端边 L2；端边外移 `min_release_clearance`、侧边拉长 `L_pull`、端边不压分叉点；中心点 `point_in_poly` 进出边沿为主判据，`entry_side` 边沿时刻锁存；不录路线顺序、不预求全序列、不引入 `NavigateThroughPoses`、不订阅 `plan`。**待明确**：`polygon` / `wait_poses` 字段名、`resource_id` 取值、`L_pull` 与 `min_release_clearance` 数值（`L_pull ≥ v_max × (RTT_拒绝_p99 + T_cancel + T_newgoal_accept) + v_max²/(2·abs(a_dec)) + d_safety`，由 V1.5 基准实验实测）、validator 7 条规则数值细节、地图包版本升级与 TBD-003 的关系。见 3.2.1 | 是 |
| TBD-022 | 电量速率参数与日志 | 三个速率数值（IDLE / WORK / CHARGING）、`mode_timeout_sec`、`low_soc_threshold` / `charge_target_soc` 双阈值；是否新增 `tbl_robot_battery_log`；仿真时间换算（充电源形态已由 TBD-035 冻结为 B、档位判定已由 TBD-036 冻结为 D，均不在本项） | 是 |
| TBD-023 | 行为树技术选型 | BT.CPP 版本与 Groot 配套；XML 路径与命名；自定义节点注册与 Blackboard 字段；重试次数 | 是 |
| TBD-024 | 软件急停接口 | 触发源、仲裁落点、是否人工复位、是否需要新增 ROS2 接口 | 是 |
| TBD-033 | 证据归档规范（引用） | Groot 截图与故障注入结果的存放路径 | 否 |
| TBD-035 | 充电源形态 | **已冻结：采用 B 方案**（本地按 `charge_pct_per_min` 时间积分；无独立充电模拟器、无远端充电源、无 `ChargeSource` 抽象）。仅剩速率数值待 TBD-022 一并确定 | 否（已关闭） |
| TBD-036 | 消耗档位判定信号 | **已冻结：采用 D 方案**——`amr_behavior_manager` 用专用、单向、不含 `battery` 的 `PowerMode` 声明 IDLE/WORK/CHARGING，`amr_power_manager` 查表积分、零判定、零 `cmd_vel` 订阅；`power_mode` / `power_level` 两个 topic 与两条 msg 须登记接口契约；V2 **不为 V4 预留**运行时耗电判定机制；V4 抢占策略缺口已转 TBD-037。决策记录见 3.3.2.2、落地细则见 3.3.2.3 | 否（已关闭） |
| TBD-038 | 按需触发的时序参数（**由「多路口流水线编排」重命名并收窄**） | **已定案**：运行时按需触发主模型、目标全程 D、**中心点 `inside` 进出边沿申请 / 释放（含掉头退出）、无 `plan` 订阅**（提前量由 `L_pull` 拉长控制区自带）、`REQUESTING` 收 `GRANTED` 零动作 / `WAITING` 收 `GRANTED` 立即重发 D、被拒 cancel（等 result 回调）+ 同侧就近等待点重导航、**`T_retry = 10 s` 固定无 jitter**、事件驱动优先于轮询、申请带 ETA + `lease_ttl`。**已关闭子项**：`ComputePath` / `FollowPath` 独立 Action Client = **不引入**；连带删除 `IsPathValid` / `path_validity_timeout` / 路径缓存 / 弧长排序 / 序列复核 / 预申请撤销 / `onPlan()` / `onPlanChanged()` / `pathIntersects*` / `D_trigger` / 改道去抖变量；「改道即释放」移除独立机制、语义由出区边沿 + TTL 承接。**待明确**：ETA 精度与 lease 生效时刻、**滞回双阈值**（内缩进入 / 外扩 `d_hyst` 或连续 N 帧时间窗）、边沿检测去抖、`lease_ttl` 数值、验收「授权提前到达零 goal 重发」阈值。见 3.2.2 / 3.2.3 / 3.6 | 是 |
| TBD-039 | AMR 侧几何加载（**由「路径求交」大幅缩小**） | 新建 `traffic_gate_loader` 读取独立 `traffic_gates.yaml` 的 4 点 `polygon` + `wait_poses`（`MapInfo.traffic_gates_yaml` 下发文件路径，`YAML::LoadFile`；**V1 `ZoneStationLoader` 不改**）；`point_in_poly(center, polygon)` 中心点进出边沿 + 滞回双阈值 + 状态过滤；`long_axis` / `centroid` / `entry_side` 锁存计算；`crossed()` 仅作侧边异常识别辅助。`map_tool` 7 条几何校验的 AMR 侧配套。**已删除**：路径与 polygon 求交、弧长排序、预期序列复核、`ComputePathToPose` 调用点、`plan` 订阅与 namespace 规则、`crossed()` 主判据。见 3.2.1.2 / 3.2.1.4 | 是 |

---

## 10. 为什么这么实现（取舍）

- **统一仲裁而非三个管理器**：货物配额、路权、充电点都是「共享资源 + 幂等申请 + 超时回收」，V1 已实现前两者的雏形。抽象成一层可复用状态机，代码量更小，且能讲清「把已有机制泛化」的架构能力。
- **有序申请防死锁而非检测回滚**：协议层消除循环等待更简单、更可验证；检测+回滚会引入额外状态与竞态。
- **BT 只管单趟执行**：任务级状态是跨进程可持久化契约，BT 的 tick 是易失运行时。分层后 V1 的 ACK / 补报 / 取消语义完全不动，风险最低，且「为什么划在这一层」本身就是面试里能讲十分钟的话题。
- **Grasp 用占位节点**：V4 换机械臂时只替换叶节点，父树不动，避免「先写死动作序列、事后再重构」。
- **急停提前到 V2**：多机 + 交通一旦存在就必须有安全边界；V4 车顶臂的安全互锁只是复用，不临时补。
- **电量优先复用既有链路**：上报→解析→入库→读取全已打通，只换数据源，避免重复建设。
- **SOC 单点持有在 AMR 侧**：电量是 BMS / 库仑计估出的本体状态，Edge 只存只读镜像、只仲裁充电点。若让 Edge 计算或由外部服务返回「SOC 增量」，会重新引入状态收敛与幂等问题，且断网时不自洽。
- **充电用 B 方案、不做接口抽象**：本地按时间速率积分即可跑通全部验收项；在没有第二个充电源实现时引入 `ChargeSource` 是空接口。刻意保持线性可复算，把复杂度留给资源仲裁与行为树；线性简化在面试时主动声明为边界。
- **电量档位由行为层声明、电量层零判定（D 方案）**：「这趟算不算工作」是行为层语义，由其用专用 `PowerMode` 单向声明，比用 `cmd_vel` 猜（需时间窗/滞回/阈值/mux 切换点，且 V4 机械臂静止抓取会漏算）简单得多；消息不含 `battery`，从结构上杜绝「消费自己输出」的回流环。代价是两个静态 msg + 两个常量，换来运行时判定逻辑几乎清零、与仿真时钟一致、可离线复算——接口是看得见的静态成本，判定是藏起来的动态成本，后者才是后期维护与面试追问易出问题的地方。
- **运行时按需触发，不预先决定走哪条路**：V1 数据里没有路线表，`GoodsPointResolver` 只解析一个目标点，路径由 Nav2 规划。旧版「任务开始 `ComputePathToPose` 求全序列」会把某个 costmap 快照下的路径当成路权顺序，形成与 `bt_navigator` 并行的第二套路径真值，并把 Nav2 的动态选路锁死。改为「路权挂在 4 点通行控制区上、**AMR 中心点 `inside` 进区边沿才申请**（方向无关）」后，端边不压分叉点 → 走绕行路的车根本不进区、零申请零停顿，A→D 有窄道常规路与无窄道绕行路时系统根本不需要知道该选哪条 —— **这正是项目所有者要的「不死板」**。「改道即释放」保留语义、移除独立机制：驶离控制区由出区边沿释放、停住 / 断流由 TTL 回收，无需 `plan` 订阅或改道去抖逻辑。
- **资源生命周期与导航 Action 生命周期解耦（INV-13，本模型下更强）**：一趟任务一个 `task_state`，路权只是沿途按需遇到的闸门；**释放靠中心点出区边沿、与导航返回结果完全解耦**，`NavigateToPose` 目标全程为 D。既不因释放重启导航，也不因导航 `SUCCEEDED` 才敢释放。否则 N 个路口就会变成 N 次「停—重规划—等 goal 接受」，吞吐量被串行编排吃掉。
- **释放靠「中心点出区边沿 + 时限兜底」，把动态检测换成静态约束**：目标改为 D 后，「释放点作为独立 goal + `SUCCEEDED` 即释放」的前提消失（且独立释放点会触发完整停顿链）。改用 AMR **中心点 `point_in_poly` 的 `inside` 出区边沿**（与进区边沿同一判据、方向无关，含掉头退出与改道驶离），不是 `crossed()` 跨线事件、不是 `footprint`；端边外移 `min_release_clearance` 把「中心出区 ⇒ 车尾已出窄道」变成录入时一次性静态几何约束，边界抖动由**边沿检测 + 滞回双阈值**（TBD-038）处理。四个触发取最早（出区边沿 / 任务结束 / `lease_ttl` 到期 / 显式 `CANCEL`），前两个保证及时、后两个保证绝不漏释放；与 D 方案「零运行时判定」是同一套思路。
- **等待时 Nav2 无活动 goal（模型 A）**：不是调参技巧，而是结构选择。`SimpleProgressChecker` 20 s 上限决定了任何「活 goal + 速度归零」的暂停方案在多车排队下必炸；让等待发生在导航段之间，风险直接从结构上消失。代价是被拒时必须 `cancel`（等 result 回调）+ 发 `NavigateToPose(同侧就近等待点)` + 获批后重发 D；但**拉长控制区自带提前量 + 进区边沿即申请让绝大多数情况走「授权到 → 零动作」分支**，只有真被拒才付这份代价，比「`plan` 预判 + 单段前瞻」更简单且停顿更少。
- **Edge 中心排队 + 显式取消 + TTL 兜底**：通配符订阅只解决「接得到」，不解决「懂不懂」，所以取消必须是一等显式消息、动作必须在 topic 层或 payload 里可辨；用 `request_id` 幂等 + `epoch` 防迟到、`lease_ttl` 兜底 AMR 崩溃 / 丢包 / 车停在区内不动 / 位姿断流 / 改道后驶离但边沿漏检。**复杂度靠边界收敛（单资源 FIFO、不做抢占与死锁检测），不靠加机制覆盖**；这三类问题（幂等 / 迟到消息 / 超时回收）恰是集成岗最值得讲的排障素材。

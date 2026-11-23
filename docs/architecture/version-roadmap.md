# 多版本开发路线图与版本边界

**Status:** Planned
**文档类型:** 内部开发任务书（非对外展示文档）
**适用对象:** V1.5 / V2 / V3 / V4 各版本实现者（可能不是本文档编写者）
**权威基线:** `docs/requirements/srs.md`（V0.20）、`docs/requirements/interface-contract.md`、`AGENTS.md`
**创建日期:** 2026-11-19
**分册:** `docs/architecture/roadmap/` 下四份，见第 10 节

> **⚠️ 2026-11-22 V0.20 定案变更**：项目所有者第十三轮拍板 8 项，V2 窄道几何与判据整体改写 —— 独立 `traffic_gates.yaml`（4 点凸 `polygon` + `wait_poses{l1,l2}`）、**AMR 中心点进出区域的 `inside` 边沿**为主判据、**全程不订阅 `plan`**、`traffic_gates_editor` 两阶段录入、改道释放语义由出区边沿 + TTL 承接。旧「`NARROW` zone / `TRAFFIC_WAIT` / 有向触发段 / 外移释放线 / `crossed()` 主判据 / `onPlan()` 预判 / `D_trigger`」口径**已废弃**。权威口径见 `.workbuddy/artifacts/traffic-gate-8-decisions-frozen.md`。本文件 INV-13、TBD-021 / 038 / 039 已按新口径改写。

---

## 0. 文档定位与规则约束

### 0.1 是什么 / 不是什么

| | 说明 |
| --- | --- |
| **是** | 各版本的目标、范围边界、功能细节、接口与数据模型要求、异常路径、验收标准、文件级改动清单、实施顺序 |
| **是** | 给实现者用的任务书，要求「不看对话记录也能开工」 |
| **不是** | 需求基线。`docs/requirements/srs.md` 仍是唯一权威需求文档，本文档不得覆盖或替代它 |
| **不是** | 已实现能力的描述。引用 V1 现状时必须标注 `Implemented` 并给出文件证据 |
| **不是** | 对外展示材料。**README 和演示视频不引用本文档及分册** |

> **为什么这么定**：本项目用于面试展示，此前评审已指出「文档数量多 ≠ 严谨」。路线图属于开发过程资产，堆到 GitHub 首页会触发「用文档冒充工程量」的怀疑。对外只保留 README、架构图、ADR 和验证报告。

### 0.2 实现者必读的项目规则（摘录自 `AGENTS.md`）

1. **版本边界**：V1 只实现 SRS 中标记 V1 且优先级 Must 的需求；V2 及后续版本规划**不得自动转化为 V1 开发任务**。
2. **文档同步**：新增或修改接口、Topic、消息字段、状态机、数据模型时，**必须同步更新 SRS、接口契约或架构文档**。
3. **TBD 规则**：尚未确定的事项必须标记为 `TBD` 并写明需要确认的具体问题；**不得为了让文档看起来完整而自行填充未知信息**。
4. **状态标注**：文档状态统一使用 `Implemented / Partial / Planned`；必须区分「已确定的架构决策」「已实现的代码能力」「仅规划但未实现的能力」。
5. **重大变更需确认**：新增/删除/变更对外接口、接口在包间迁移、架构或部署调整等，必须先向项目所有者说明方案并征得同意，**禁止直接执行**。
6. **不破坏既有能力**：不删除现有功能；不为统一风格大规模重构现有代码；不擅自修改构建文件、依赖配置或业务代码，除非任务明确允许。
7. **说明动机**：每次完成功能或文档后必须说明「为什么这么实现」，包含设计动机、边界和取舍，便于面试表达和后续维护。
8. **提交规范**：提交说明应关联需求编号、验收项和必要文档。

### 0.3 本文档的写法约定

- 所有版本条目默认状态为 `Planned`。引用 V1 已有代码时标 `Implemented` 并给出文件路径。
- 「**建议方案**」表示本路线图推荐但未冻结的技术选型，实施前需按第 0.2 条第 5 款确认。
- 「**TBD-0xx**」表示必须确认后才能开工的事项，统一登记在第 9 节与各分册末尾。
- 编号 `TBD-001` ~ `TBD-019` 已被 SRS 第 9 章占用，本文档新增编号从 `TBD-020` 起；已有事项（如 Cloud 形态）**直接引用原编号，不重复新建**。
- 新文件一律使用 **LF 换行**、UTF-8 编码；C++/Python/ROS 接口/CMakeLists/YAML 遵循 `.gitattributes` 强制 LF。

---

## 1. 版本总览

| 版本 | 主题 | 状态 | 前置依赖 | 一句话目标 | 验收门槛 |
| --- | --- | --- | --- | --- | --- |
| **V1** | 任务编排与状态同步 | `Implemented` | — | 已交付：Edge HTTP 任务注入、AMR 行为状态机、多机隔离、断网待补报与幂等表 | 见 `docs/development/v1-acceptance-checklist.md`；AC-011~014 未验证 |
| **V1.5** | 验证基线 | `Planned` | V1 | **不新增任何业务功能**，只补测试证据与可观测性 | AC-011~014 全部实测通过并归档证据 |
| **V2** | 资源仲裁 + 执行层骨架 | `Planned` | V1.5 | 交通路权、电量真实化、行为树执行骨架、软件急停 | 两车路口不冲突、低电量自动回充、BT 可组合恢复可演示 |
| **V3** | 云端 + Web + 物理台阶 | `Planned` | V2 | Cloud 通过 MQTT 下发任务、只读 Web 看板、货箱物理挂载 | 云端下发的任务端到端跑通，货箱可见地随车移动 |
| **V4** | 车顶机械臂（移动操作） | `Planned` | V3 | 停稳后抓取，替换行为树 `Grasp` 叶节点 | 真实抓取成功/失败可观测、不翻车、多机隔离保持 |

**不设 V5。** 行为树并入 V2，理由见第 3 节。

> **为什么把 V1.5 单独列出来**：SRS 10.3 节「V1 完成定义」明确要求「达到条件后，应先整理项目成果、测试证据和文档，再决定是否启动 V2，而不是继续无边界地向 V1 添加功能」，且任务取消、断网续行、结果补报均在完成条件内，但 `v1-acceptance-checklist.md` 中 AC-011~014 标注为「Planned，本轮不测试」。V1.5 是对 SRS 完成条件的补齐，**不是新增版本功能**，因此不违反「V2 规划不得转为 V1 任务」。

---

## 2. 版本边界总则

### 2.1 每个版本都允许做

- 新增该版本范围内的功能、接口、数据表、节点、配置项。
- 新增测试脚本、故障注入工具、基准实验脚本（属于验证设施，不算业务功能）。
- 同步更新 README、架构图、演示素材与该版本涉及的既有文档。

### 2.2 每个版本都禁止做

- 删除或改写 V1 已验证的业务链路（注册、地图包、任务注入、货物申请、状态上报、ACK、待补报）。
- 改变第 3 节列出的**跨版本不变式**，除非走第 0.2 条第 5 款的重大变更确认流程。
- 在 V1/V1.5 阶段实现 V2 及以后才属于范围的功能（V2 路权逻辑、电量管理、Cloud Platform、机械臂、视觉、完整 WMS/Web 平台）。
- 为「统一风格」大规模重构既有代码。
- 把技术选型中标记为 `TBD` 或「建议方案」的内容当作已冻结决策直接落地。

### 2.3 各版本明确的 Out of Scope

| 版本 | 明确不做 |
| --- | --- |
| V1.5 | 任何新业务功能、任何对外接口/消息字段变更、任何状态机改动 |
| V2 | Cloud Platform、Web 界面、机械臂、视觉识别、完整 WMS、高级任务调度（8.3）、MySQL/Redis |
| V3 | 机械臂抓取、移动中操作、完整 WMS、权限审计、生产级高可用、集群与跨仓库容灾 |
| V4 | 移动中抓取、多臂协同、力控装配、SLAM 重定位、算法类研究、真实硬件驱动 |

---

## 3. 跨版本不变式（最重要的一节）

以下约定在 V1.5 ~ V4 全程**不得改变**。实现者若发现某版本设计需要破坏其中一条，必须先提出方案并征得项目所有者同意。

| 编号 | 不变式 | 依据 / 证据 |
| --- | --- | --- |
| INV-01 | **三套状态不可混用**：任务级 `state`（Edge）、AMR 行为 `task_state`（AMR）、机器人运行 `RobotState`（Edge） | `docs/architecture/task-state-machine.md` 第 1 节 |
| INV-02 | **`task_state` 状态机已冻结**，枚举与转移不得因引入行为树而改变 | `AGENTS.md` 第 5 节；SRS D-035 / D-036 |
| INV-03 | 对外 `task_state` 上报、5 秒周期、`status/ack`、待补报、取消语义保持不变 | AGENTS.md 第 3 节；`interface-contract.md` 4.6 / 4.10 |
| INV-04 | MQTT Topic 常量统一定义在 `common/mqtt/mqtt_topic.h`；ROS2 接口名统一定义在 `robot_ws/src/amr_common/include/amr_common/ros_interface_names.h` | AGENTS.md 第 3 节 |
| INV-05 | 消息 JSON 字段使用 `snake_case`；任务相关 MQTT Topic 使用 QoS 1 | AGENTS.md 第 3 节；ADR-0001 |
| INV-06 | 目的地**不下发坐标**，由 `goods_id + task_type` 经 `map -> zone -> station -> location` 推导 | SRS D-025 / D-048 / D-050 |
| INV-07 | `goods_status` 四量不变式：`total = in_progress + completed + pending` | `tbl_task_goods_status` 的 CHECK 约束；AGENTS.md 第 3 节 |
| INV-08 | 幂等合并语义不变：`INSERT OR IGNORE INTO tbl_amr_task_status_event` + `UNIQUE(message_id, task_id, robot_id, event_type)` | `edge_server/src/database/sqlite_db.cpp:1292` |
| INV-09 | 本地待补报采用**原子写入**（`.tmp` + `rename`），收到 ACK 后清理 | `robot_ws/src/amr_agent/src/persist/pending_report_store.cpp`；SRS D-028 |
| INV-10 | AMR 内部 ROS2；AMR↔Edge HTTP/MQTT；Simulation↔Edge WebSocket；**Cloud 侧与 Edge 侧为两套独立 EMQX** | AGENTS.md 第 3 节；`docs/architecture/system.md` |
| INV-11 | 多机 TF 帧使用 `robot_id + "_"` 前缀（`map` 除外），Nav2 参数按 `robot_id` 动态生成 | SRS D-056；`robot_ws/src/amr_navigation/scripts/start_nav2.py` |
| INV-12 | 模块之间通过明确接口交互，不直接依赖其他模块内部实现；ROS2 包单一职责，禁止跨包耦合 | AGENTS.md 第 3 节；各包内 `AGENTS.md` |
| INV-13 | **交通资源生命周期 ≠ 导航 Action 生命周期**：一趟任务一个 `task_state`，路权是沿途**运行时按需遇到**的闸门，不因路权点切分任务、不重启导航流程；`NavigateToPose` 目标全程为 D，**申请 / 释放由 AMR 中心点进出 4 点通行控制区的 `inside` 边沿触发（边沿检测 + 滞回），与导航返回结果解耦**；**全程不订阅 `plan`**，提前量由区域沿通道拉长 `L_pull` 自带；只录几何（独立 `traffic_gates.yaml` 的 4 点 `polygon` + `wait_poses{l1,l2}`），**不人工录入有序路线表、不预先求全序列、不下发途经点**，Nav2 动态选路能力必须保留 | V2 分册 3.2.1 / 3.2.2 / 3.2.3；项目所有者 2026-11-20 第五轮定案，2026-11-22 第十三轮按 V0.20 修订 |
| INV-14 | **V2 路权逻辑不设独立模块**：实现为 `amr_behavior_manager` 包内的 `TrafficGate`（类 + BT 节点），全仓禁止再使用已废弃的独立交通模块命名；专用 ROS2 topic 固定为 `traffic_request` / `traffic_response`（消息 `TrafficRequest.msg` / `TrafficResponse.msg`），**不复用** `task_command` / `task_event`；MQTT 固定为 `amr/{robot_id}/traffic_rights/request` / `response`，动作由 payload `op` 承载、不采用动作分层 topic；`amr_agent` 只做 `TrafficBridge` 透明桥，不持有位姿 / 几何 / 业务决策 | 项目所有者 2026-11-20 第七轮定案；V2 实现规格 §1 / §3、控制链 §3 |

---

## 4. 依赖链与实施顺序

```text
V1 (已交付)
  └─ V1.5 验证基线 ──── 补齐证据与可观测性，不新增功能
       └─ V2 资源仲裁 + BT 骨架 ──── 执行层骨架在此落地
            └─ V3 云端 + Web + 货箱挂载 ──── 观察层与物理台阶
                 └─ V4 车顶机械臂 ──── 只替换 BT 的 Grasp 叶节点
```

### 4.1 关键依赖说明（为什么是这个顺序）

1. **V1.5 必须先于 V2**：V2 引入交通与电量后，异常路径数量显著增加；若 V1.5 没有建立故障注入与取证能力，V2 的验证只能靠手工，会重演「代码有实现、拿不出证据」的问题。
2. **行为树必须在 V2 落地，不能推迟到机械臂之后**：行为树处理的是「失败怎么办、谁优先、何时等待」，这些问题在只有底盘时就全部存在（导航失败、路权等待、低电量）。V2 的交通与电量恰好是行为树的两个条件节点。若 V2/V3/V4 先用 if-else 写死恢复逻辑，再统一重构，等于三次硬编码 + 一次大重构，违反「不大规模重构」规则。
3. **机械臂是叶节点替换，不是树重构**：V2 的 `Grasp` 为 `Wait` 占位节点，V4 换成 `GraspAction`（内部接 MoveIt / 夹爪 / 接触检测），父树、Guard、Fallback、任务级 FSM 全程不动。这是把行为树放在 V2 的核心收益。
4. **Cloud 与 Web 合并到 V3**：按 ADR-0001，Cloud 只是把任务入口从 Edge HTTP 换成 MQTT 发布，功能增量最小；Web 是同一条链的数据消费方，拆成两版会出现「有后端没画面」或「有画面没入口」。
5. **货箱挂载放在 V3、机械臂放在 V4**：先把「货物在仿真里物理存在且能随车移动」做掉，V4 只引入「谁把它拿起来」这一个新变量，避免一次性引入臂、夹爪、货物模型、接触检测四个新变量。

---

## 5. 每版通用 Definition of Done

任一版本宣告完成前，以下全部满足：

- [ ] 该版本「验收清单」中所有条目实测通过，证据归档（日志片段、SQLite 前后对比、GIF/截图）。
- [ ] 所在 ROS2 包可独立 `colcon build --packages-select <pkg>` 通过；Edge Server 可 `cmake --build` 通过。
- [ ] 相关 launch / 节点可启动，Gazebo + Nav2 + Edge 全链路至少跑通一次该版本主场景。
- [ ] 第 3 节所有不变式逐条复核，未破坏。
- [ ] 按第 6 节完成文档同步；SRS / 接口契约 / 架构文档状态词与代码一致。
- [ ] README、架构图、演示素材更新到该版本。
- [ ] 提交说明关联需求编号、验收项与文档；附「为什么这么实现」说明。
- [ ] 该版本 TBD 中标注「实现前必须明确」的条目已关闭或显式转为下一版本 TBD。

> 注：本机为 Windows，ROS2 编译与仿真验证需在 WSL / Ubuntu 环境完成。文档编写者不承诺在本机完成编译验证。

---

## 6. 文档同步清单（按版本）

| 版本 | 需同步的既有文档 | 同步内容 |
| --- | --- | --- |
| V1.5 | `docs/development/v1-acceptance-checklist.md`、`docs/TODO.md`、`docs/README.md` | AC-011~014 勾选与证据链接；校准 SRS 版本号与状态词 |
| V2 | `docs/requirements/srs.md` 第 8 章、`interface-contract.md` 第 4/7 节、`docs/architecture/edge-data-model.md`、`amr-application-design.md`、`task-state-machine.md`、`system.md`、`AGENTS.md` 第 5 节 | 新增资源仲裁接口与数据表、BT 分层说明、电量字段语义、冻结边界的补充说明；新增 ADR |
| V3 | `srs.md` 第 8.4 节、`interface-contract.md` 第 4 节、`system.md`、ADR-0001、`docs/README.md` | Cloud↔Edge MQTT 契约、Web 数据源、TBD-017 状态更新 |
| V4 | `srs.md` 第 2.4/8 章、`system.md`、`amr.md`、`amr-application-design.md`、`interface-contract.md` | 机械臂模型与控制接口、抓取状态语义、安全互锁约束 |

**重要**：按 `AGENTS.md`，`docs/requirements/srs.md` 为需求基线，未经明确授权不得改动。上表列出的是「实施时应当同步」的目标，**不代表本次编写已修改 SRS**；本轮只新增路线图文档与一份索引条目。

---

## 7. 给实现者的通用工程规则

| 项 | 规则 |
| --- | --- |
| 换行符 | 新建 C++/Python/ROS 接口/CMakeLists/YAML/sh 一律 LF；`.gitattributes` 已强制 |
| 编码 | 全部 UTF-8 |
| C++ 风格 | 以 `robot_ws/src/amr_navigation/src/navigation_server.cpp` 为范本：Tab 缩进、Allman 大括号、`m_` 成员前缀、`this->` 显式调用、`Type var{};` 值初始化 |
| 依赖声明 | `package.xml` 的 `<depend>` 必须与 `CMakeLists.txt` 的 `find_package` 一致；新增依赖不得隐式依赖系统库 |
| 命名 | 包名/节点/话题/参数一律小写下划线；JSON 字段 `snake_case` |
| 接口变更 | 修改对外接口（topic/service/action/参数名/数据结构）视为破坏性变更，**必须同步所有调用方** |
| 单一职责 | 每个功能包单一职责，禁止跨包耦合；新节点/可执行文件必须注册到 `setup.py` 或 CMakeLists 的 install 目标 |
| 验证 | 以手动仿真为主：Gazebo 启动 → Nav2 激活 → 运行目标节点 → RViz 观察；当前各包仅有 lint 测试，无单元测试 |
| 动机说明 | 每个功能提交时说明设计动机、边界、取舍 |

---

## 8. 现状资产索引（V1 已实现，供各版本复用）

| 资产 | 位置 | 复用方式 |
| --- | --- | --- |
| 货物配额原子扣减 | `edge_server/src/database/sqlite_db.cpp` 的 `ApproveGoodsAllocation` | V2 资源仲裁的原子扣减范式 |
| 配额结算/回退 | `SettleGoodsAllocation`、`tbl_task_goods_allocation` 的 `UNIQUE(request_id, robot_id)` | V2 lease 的幂等与状态流转 |
| 充电点分配 | `edge_server/src/service/idle_point_allocator.cpp`（212 行），SRS D-053/D-054 | V2 充电点资源化 |
| 幂等事件表 | `tbl_amr_task_status_event`，`sqlite_db.cpp:1292` | 全版本保持不变 |
| 待补报原子写 | `robot_ws/src/amr_agent/src/persist/pending_report_store.cpp` | 全版本保持不变 |
| 路权 Topic（V2 已定名） | `common/mqtt/mqtt_topic.h` 现为 `TRAFFIC_RIGHTS_REQ` / `TRAFFIC_RIGHTS_RSP_PREFIX`（无 `robot_id` 层，与通配订阅不自洽） | V2 **替换**为 `AMR_TRAFFIC_REQUEST` = `amr/{}/traffic_rights/request`、`AMR_TRAFFIC_RESPONSE` = `amr/{}/traffic_rights/response`、`AMR_TRAFFIC_SUBSCRIBE` = `amr/+/traffic_rights/request`；Edge FIFO 排队 + 显式 `cancel` + `lease_ttl` 回收（V2 分册 3.2.5） |
| 行为状态机 | `robot_ws/src/amr_behavior_manager/src/amr_behavior_manager_node.cpp`（680 行） | V2 将单趟执行段迁移到 BT，任务级状态不动 |
| 多机 Nav2 参数生成 | `robot_ws/src/amr_navigation/scripts/start_nav2.py` | 全版本保持不变 |
| 生命周期状态机 | `simulation_ws/src/simulation_manager/src/robot_lifecycle_manager.cpp` | V1.5 故障注入的注入点之一 |
| 排查案例 | `docs/development/troubleshooting-navigation.md`（342 行）、`troubleshooting-dynamic-obstacle.md`（144 行）、`nav2-tuning-notes.md`（150 行 + 3 份参数快照） | V1.5 故障注入场景与面试讲述素材 |
| 评审结论 | `docs/development/project-review.md` | 各版本边界与 ROI 排序的参考 |

---

## 9. TBD 总表

`TBD-001` ~ `TBD-019` 见 SRS 第 9 章。以下为本路线图新增，编号从 `TBD-020` 起；每项均写明「需要明确的具体问题」，未确认前不得假设实现方式。

| 编号 | 所属版本 | 问题 | 需要明确的具体内容 | 开工前必须明确 |
| --- | --- | --- | --- | --- |
| TBD-020 | V2 | 资源仲裁统一接口与 Edge 排队 | **已定案**：中心排队在 Edge、FIFO、显式取消为一等事件、`lease_ttl` 超时回收、`request_id` 幂等 + `epoch` 防陈旧、通配符订阅且动作可辨、复杂度边界；**2026-11-20 第七轮追加定案**：模块归属（INV-14）、ROS2 topic `traffic_request` / `traffic_response`、MQTT topic 带 `robot_id`、动作由 payload `op` 承载（**不采用动作分层 topic**）、消息字段集合。**待明确**：是否新增 `tbl_resource_lease` 表；`queue_seq` 生成方式、`PENDING` 超时作废时长、`lease_ttl` 默认值与续租规则、表最终字段；与 `tbl_task_goods_allocation` 的一致性策略。见 V2 分册 3.1.2 / 3.2.5、V2 实现规格 §1 | 是 |
| TBD-021 | V2 | 窄道建模与触发/释放几何 | **已定案（V0.20 修订）**：独立 `traffic_gates.yaml`，一条窄道一条记录 = `resource_id` + 4 点凸 `polygon`（点序 `P1→P2` 端边 L1 / `P2→P3` 长轴 / `P3→P4` 端边 L2）+ `wait_poses{l1,l2}`（每侧 ≥1）；**无** `side` / `forward` / `trigger` / `release` 字段，**不扩展** `zones.yaml` / `stations.yaml` 的 `type`、不新增 `NARROW` / `TRAFFIC_WAIT`；区域为「端边外移 `min_release_clearance` + 侧边拉长 `L_pull`」的通行控制区而非 3×2 物理窄道，端边不压分叉点（保 S6 零申请）；**中心点进区边沿申请 / 出区边沿释放**，`SUCCEEDED` 降为兜底；进入侧由 `dot(enter_pose-centroid, long_axis)` 符号定并锁存，等待点在**同侧组内**就近选取且位于入口端边外侧；不录路线顺序、不引入 `NavigateThroughPoses`。**待明确**：`L_pull` / `min_release_clearance` / `d_safety` 数值（V1.5 基准实验实测）、地图包版本升级（TBD-003）、`map_tool` 7 条内容校验规则细节。见 V2 分册 3.2.1、接口契约 §8.7 | 是 |
| TBD-022 | V2 | 电量速率参数与日志 | 三个速率数值（IDLE / WORK 消耗、充电）、`mode_timeout_sec`、`low_soc_threshold` / `charge_target_soc` 双阈值；是否新增 `tbl_robot_battery_log`；仿真时间换算（充电源形态已由 TBD-035 冻结为 B、档位判定已由 TBD-036 冻结为 D，均不在本项） | 是 |
| TBD-023 | V2 | 行为树技术选型 | BehaviorTree.CPP 版本（Humble Nav2 对应 3.x）、XML 文件命名与存放路径、自定义节点注册方式、Groot 配套版本 | 是 |
| TBD-024 | V2 | 软件急停接口 | 触发源（HTTP / MQTT / ROS2 topic）、`cmd_vel` 仲裁落点（新节点还是 `navigation_server` 内）、恢复语义与是否需要人工复位 | 是 |
| TBD-025 | V1.5 | `trace_id` 落地形态 | 是否进入 MQTT 消息字段（属接口变更，需同步契约）还是仅存在于日志（零侵入）；跨进程传播格式 | 是 |
| TBD-026 | V3 | Web 技术栈与部署 | 前端框架、是否独立进程、静态资源托管位置、只读边界的鉴权方式 | 否（可在设计阶段确定） |
| TBD-027 | V3 | 双 EMQX 连接细节 | Cloud 侧与 Edge 侧 broker 的地址、凭据注入方式、topic 映射与桥接策略 | 是 |
| TBD-028 | V3 | 货箱模型与挂载 | 货箱模型来源（新 SDF 还是复用 world）、`attach/detach` 插件选型、载重是否影响底盘动力学 | 是 |
| TBD-029 | V3 | 云端是否改变选机语义 | 是否仍要求 `robot_ids` 非空、是否允许云端/Edge 自动选择参与 AMR（V1 明确不支持） | 是 |
| TBD-030 | V4 | 机械臂形态与控制 | 自由度、夹爪类型、MoveIt2 还是预定义关节轨迹、是否引入深度相机 | 是 |
| TBD-031 | V4 | 抓取成功判定 | 接触检测 / 夹爪闭合反馈 / 超时判定；抓取失败的重试与回退策略 | 是 |
| TBD-032 | V4 | 车顶臂对模型与 TF 的影响 | mass/inertia 重调范围、`base->arm` TF 链、多机 namespace 下 MoveIt 配置生成方式 | 是 |
| TBD-033 | 跨版本 | 演示与证据归档规范 | 每版日志片段、SQLite 前后对比、GIF/截图、验证报告的存放路径与命名 | 否 |
| TBD-034 | 跨版本 | 回归测试基线 | 是否引入 gtest / 集成测试框架；最小自动化用例集范围 | 否 |
| TBD-035 | V2 | 充电源形态 | **已冻结：采用 B 方案**（`amr_power_manager` 本地按 `charge_pct_per_min` 时间积分；无独立充电模拟器、无远端充电源、无 `ChargeSource` 抽象）。仅剩速率数值待 TBD-022 一并确定 | 否（已关闭） |
| TBD-036 | V2 | 消耗档位判定信号 | **已冻结：采用 D 方案**——`amr_behavior_manager` 用专用、单向、不含 `battery` 的 `PowerMode` 声明 IDLE/WORK/CHARGING，`amr_power_manager` 查表积分、零判定、零 `cmd_vel` 订阅；`power_mode` / `power_level` 两个 topic 与两条 msg 须登记接口契约；V2 不为 V4 预留运行时耗电判定机制；V4 抢占策略缺口已转 TBD-037。决策记录见 V2 分册 3.3.2.2、落地细则见 3.3.2.3 | 否（已关闭） |
| TBD-037 | V4 | 机械臂抓取途中的低电量抢占策略 | BT `ReactiveFallback(BatteryOK?)` 在臂动作执行中电量跌破阈值时，是否允许打断臂动作回充；与安全互锁「急停必须能中断臂动作」的边界；如需体现臂耗电，是否采用 D 方案的 `MODE_ARM_ACTIVE` 扩展（加 1 枚举 + 1 行配置）。见 V4 分册 3.6 | 是 |
| TBD-038 | V2 | 按需触发的时序参数（由「多路口流水线编排」重命名并收窄） | **已定案（V0.20 修订）**：运行时按需触发主模型、目标全程 D、**全程不订阅 `plan`**（提前量由拉长 `L_pull` 自带，`D_trigger` 概念转译为 `L_pull`）、被拒 cancel + 等 result 回调 + 同侧等待点重导航、**`T_retry = 10 s` 固定无 jitter**、事件驱动优先于轮询、申请带 ETA + `lease_ttl`；`REQUESTING` 收 `GRANTED` 零动作、仅 `WAITING` 获批才重发 D。**已关闭子项**：`ComputePath` / `FollowPath` 独立 Action Client、`onPlan()` / `onPlanChanged()` / `pathIntersects*` / 改道去抖冷却、`IsPathValid` / `path_validity_timeout` / 路径缓存 / 弧长排序 / 序列复核 —— **均不引入或已移除**。「改道即释放」保留语义、移除独立机制（由出区边沿 + TTL 承接）。**待明确**：ETA 精度与 lease 生效时刻、`L_pull` 数值、`inside` 滞回双阈值与去抖窗、`lease_ttl` 数值、验收阈值。见 V2 分册 3.2.2 / 3.2.3 / 3.6 | 是 |
| TBD-039 | V2 | AMR 侧几何加载与区域判据（由「路径求交」大幅缩小，V0.20 再修订） | 新建 `traffic_gate_loader` 读取独立 `traffic_gates.yaml` 的 4 点 `polygon` 与 `wait_poses{l1,l2}`（经 `MapInfo.traffic_gates_yaml` 下发）；`inside = point_in_poly(center, polygon)` 边沿检测 + 滞回双阈值、`entry_side` 锁存与同侧就近、`already_handled` 防重复；`crossed()` 降级为「侧边异常穿入」识别辅助；**无 `plan` 订阅、无 namespace 上的 Path 订阅规则**；`map_tool` 7 条几何校验的 AMR 侧配套。**已删除**：路径与 polygon 求交、弧长排序、预期序列复核、`ComputePathToPose` 调用点、有向线段 `crossed()` 主判据。见 V2 分册 3.2.1.2 / 3.2.1.4、实现规格 §5.1 / §5.3 | 是 |

> 已有事项直接引用原编号，不重复登记：**TBD-017**（Cloud Platform 后续形态：CLI / HTTP 脚本 / FastAPI 未定）、**TBD-004**（任务取消的安全策略，V1.5 验证时需给出最低可行规则）、**TBD-009**（断网期间的任务取消）、**TBD-010**（任务状态冲突合并规则）、**TBD-019**（Edge 故障恢复细节）。

---

## 10. 分册索引

| 分册 | 文件 | 内容 |
| --- | --- | --- |
| V1.5 验证基线 | [`roadmap/v1.5-validation-baseline.md`](roadmap/v1.5-validation-baseline.md) | AC-011~014 实测、故障注入框架、trace_id、基准实验、文档校准 |
| V2 资源仲裁与行为树 | [`roadmap/v2-resource-arbitration-and-bt.md`](roadmap/v2-resource-arbitration-and-bt.md) | 统一资源仲裁、交通权限、电量、BT 骨架、软件急停、降级矩阵 |
| V3 云端 Web 与物理交互 | [`roadmap/v3-cloud-web-and-physical-goods.md`](roadmap/v3-cloud-web-and-physical-goods.md) | Cloud Platform、Web 看板、货箱 attach/detach、可选项 |
| V4 车顶机械臂 | [`roadmap/v4-mobile-manipulation.md`](roadmap/v4-mobile-manipulation.md) | 移动操作、Grasp 叶节点替换、安全互锁、模型与控制器 |

---

## 11. 版本划分的取舍说明（便于面试表达）

- **为什么按「能力层次」而不是「功能域」切版本**：交通、电量、货物配额本质是同一类共享资源问题，合并成统一仲裁层可复用 V1 的原子扣减、幂等表和超时回收，比为每个功能写一个管理器更省代码，也更能讲清架构抽象。
- **为什么行为树并入 V2 而不是单列一版**：行为树不是功能，是执行层骨架。它承载的正是 V2 新增的两个条件逻辑；单列一版会导致前几版把恢复逻辑写死再返工。
- **为什么机械臂放最后**：它引入的是另一条技术栈（运动学 / 控制集成），与 V1 的分布式状态一致性主线几乎不共享代码；且 Gazebo 物理抓取风险高，需要前面的验证设施与安全互锁先就位。
- **为什么坚持每版都有验收门槛**：本项目用于面试，面试官区分「demo」与「工程」主要看异常处理是否被验证过。功能数量不提升可信度，可复现的证据才提升。

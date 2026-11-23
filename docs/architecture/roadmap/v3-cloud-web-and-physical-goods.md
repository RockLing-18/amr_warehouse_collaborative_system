# V3 云端 / Web / 物理交互 · 开发任务书

**Status:** Planned
**所属路线图:** [`../version-roadmap.md`](../version-roadmap.md)
**版本主题:** Cloud Platform 任务入口 + 只读 Web 看板 + 货箱物理挂载（机械臂前置台阶）
**前置依赖:** V2 完成（资源仲裁、行为树、电量、急停均可用）

---

## 1. 目标

把任务入口从 Edge 侧 HTTP 升级到云端 MQTT 发布（V1 HTTP 注入保留为兼容接口），提供可观察的 Web 界面，并让货物在仿真里**物理存在且能随车移动**——为 V4 的机械臂抓取铺好台阶，使 V4 只引入「谁把货物拿起来」这一个新变量。

> **为什么合并成一版**：按 ADR-0001，Cloud 只是把入口从 HTTP 换成 MQTT 发布，功能增量最小；Web 是同一条链的数据消费方。拆成两版会出现「有后端没画面」或「有画面没入口」。货箱挂载放在同一版，是为了让这一版有可见的演示产出，同时把 V4 的风险前置消化。

---

## 2. 范围边界

### 2.1 In Scope

- Cloud Platform：任务创建入口、MQTT 下发、任务状态回传、与 Edge 的连接。
- Web 端：只读状态看板 + 手动下发/取消任务（明确边界，见 3.2）。
- 货箱模型与 `attach/detach`：货物随车移动。
- 可选项（标注为非必须，见 3.4）。
- 与 V2 行为树、资源仲裁的联动展示。

### 2.2 Out of Scope（严格禁止）

| 不做 | 归属 |
| --- | --- |
| 机械臂、夹爪、MoveIt、抓取动作、移动中操作 | V4 |
| 完整 WMS、MySQL / Redis、历史统计平台、权限审计、生产级高可用、集群与跨仓库容灾 | SRS 8.4 更后 |
| 高级任务调度（SRS 8.3：按机器人状态自动选机、任务优先级） | 更后 / TBD-029 |
| 视觉识别算法研究、SLAM、路径规划算法 | 岗位与版本均不涉及 |
| 改变 V1/V2 已冻结的契约（见路线图第 3 节 INV-01~12） | 冻结 |
| 删除 V1 的 `POST /api/tasks` 注入接口 | 必须保留为兼容接口 |

### 2.3 一条必须先解决的架构问题

`AGENTS.md` 第 3 节规定「Cloud 侧与 Edge 侧是两套独立 EMQX」（INV-10），而 ADR-0001 决策 2 规定「后续 Cloud 通过 MQTT 发布任务、Edge 订阅」。**两套独立 broker 之间的连接方式目前未定义**，见 **TBD-027**。实现者不得假设「配置同一个 broker 地址」即可，也不得在未确认前改动 EMQX 部署拓扑。

**建议方案（待 TBD-027 确认）**：Edge Server 内新增独立的 `cloud_mqtt_client`，以独立客户端身份连接 Cloud 侧 EMQX，订阅 `warehouse/{warehouse_id}/task`；收到消息后转交 Edge 内部 `TaskService`，再由 Edge 侧 EMQX 下发到 `amr/{robot_id}/task`。两条链路在 Edge 内部汇合，Cloud 不直接接触 AMR。

---

## 3. 功能细节

### 3.1 Cloud Platform

#### 3.1.1 已冻结的依据

| 依据 | 内容 |
| --- | --- |
| `AGENTS.md` 第 3 节 | V1 使用 Edge 侧 HTTP API 注入任务；后续 Cloud 通过 MQTT 发布任务，Edge 订阅任务 Topic |
| `AGENTS.md` 第 3 节 | 任务 MQTT Topic 使用 `warehouse/{warehouse_id}/task` 和 `warehouse/{warehouse_id}/task/status`，**QoS 1** |
| ADR-0001 决策 2 | 后续 Cloud 通过 MQTT 发布任务、Edge 订阅；**保留 V1 的 HTTP 注入接口** |
| `common/mqtt/mqtt_topic.h` | `WAREHOUSE_TASK`、`WAREHOUSE_TASK_STATUS` 常量已存在，含 `makeWarehouseTaskTopic()` / `makeWarehouseTaskStatusTopic()` |
| `interface-contract.md` 4.2 / 4.9 | 已定义 Cloud → Edge 任务消息与 Edge → Cloud 任务状态消息 |

#### 3.1.2 形态未定（TBD-017，禁止自行填充）

SRS **TBD-017** 明确：Cloud Platform 后续采用 **CLI / HTTP 脚本 / FastAPI 服务** 交付，以及任务创建参数如何输入，**均尚未确定**。本任务书不指定形态；实现前必须关闭 TBD-017 并同步 SRS 第 9 章。

> **为什么强调**：此前评审已指出「不得为了文档完整自行填充未知信息」。把 Cloud 写成 FastAPI 或写成 Web 后端都属于替项目所有者做决策，按 `AGENTS.md` 属于违规。

#### 3.1.3 功能要求（与形态无关的部分）

| 项 | 要求 |
| --- | --- |
| 任务下发 | Cloud 向 `warehouse/{warehouse_id}/task` 发布任务，QoS 1，消息结构遵循 `interface-contract.md` 4.2 |
| 消息幂等 | Cloud 侧需带稳定 `task_id` / `request_id`；Edge 订阅后按 `task_id` 幂等创建，**重复投递不得生成重复任务** |
| 状态回传 | Edge 向 `warehouse/{warehouse_id}/task/status` 上报任务级状态，结构遵循 4.9；任务级 `INTERRUPTED` 仍遵守「仅当参与任务的全部 AMR 异常时上报」 |
| 兼容接口 | `POST /api/tasks`、`GET /api/tasks/{id}`、`POST /api/tasks/{id}/cancel` **全部保留且行为不变** |
| 共享状态 | Edge → AMR 的 `warehouse/{warehouse_id}/task/{task_id}/state` 与 `goods_status` 四量不变（INV-07） |
| 目的地 | 仍不下发坐标，由 `goods_id + task_type` 推导（INV-06） |
| 选机语义 | V1 规定 `robot_ids` 非空且不支持 Edge 自动选 AMR。**云端是否放宽由 TBD-029 决定**，未确认前保持 V1 语义 |
| 双 EMQX | Cloud 侧与 Edge 侧 broker 隔离（INV-10）；连接与凭据注入见 TBD-027 |
| 安全 | 沿用 TBD-006（MQTT 最小可行认证）的结论；凭据不得写入仓库（V1 已外置 `AMR_MQTT_PASSWORD` 到 `.env`，Cloud 侧同理） |

#### 3.1.4 幂等与恢复要求

- Cloud 重发同一 `task_id` → Edge 返回已存在任务，不重复下发 AMR、不重复占用配额。
- Edge 重启后从 SQLite 恢复任务状态，Cloud 可通过 `task/status` 或查询接口获取最终状态（沿用 TBD-019 的恢复语义：恢复后不自动继续，需人工或上层重新创建任务）。
- Cloud 与 Edge 之间断连恢复后，未确认的任务需有明确重试/去重规则（写入 `interface-contract.md`）。

---

### 3.2 Web 端（只读看板 + 有限操作）

#### 3.2.1 定位

**不是完整 WMS，是观察窗与演示仪器。** 允许两类写操作：手动下发任务、取消任务；除此之外一律只读。这样既让演示视频「从看日志变成看画面」，又不把范围扩大到业务平台。

#### 3.2.2 数据源（现成，不新增后端接口为主）

| 数据 | 来源 |
| --- | --- |
| 任务状态与 `goods_status` | `GET /api/tasks/{task_id}`（已实现，`interface-contract.md` 3.2） |
| 生效地图 | `GET /api/maps/active`、`GET /api/maps/download` |
| 机器人清单 / 生命周期 | WebSocket `:9000` 的 `robot_list`；`tbl_robot_status`、`tbl_robot_instance` |
| 电量（V2 后） | `tbl_robot_status.battery` |
| 资源占用（V2 后） | `tbl_resource_lease`（V2 新增，以实际表名为准） |
| 行为树状态（V2 后） | Groot / BT 日志（若提供只读查询） |
| 故障注入（V1.5 后） | `tool/fault_injection/` 的脚本触发入口 |

#### 3.2.3 页面清单（最小集）

| 页面 / 面板 | 内容 | 是否必须 |
| --- | --- | --- |
| 仓库总览 | 地图底图、机器人位置、路口/充电点占用状态 | 必须 |
| 机器人列表 | `robot_id`、在线状态、`task_state`、电量、当前任务 | 必须 |
| 任务详情 | 任务级 `state`、`goods_status` 四量、参与 AMR、事件时间线 | 必须 |
| 任务下发 / 取消 | 表单（沿用 `robot_ids` 非空约束） | 必须 |
| 行为树 / 资源面板 | 当前 tick 节点、lease 占用 | 建议 |
| 故障注入面板 | 触发 V1.5 注入场景并展示断言结果 | 建议 |
| 历史统计 / 报表 | — | **不做**（SRS 8.4 更后） |

#### 3.2.4 技术栈与部署（TBD-026）

前端框架、是否独立进程、静态资源托管位置、只读边界的鉴权方式未定。建议优先选择**零构建依赖**的方案（静态 HTML + 现有 HTTP/WebSocket 接口），避免为了看板引入新的构建链与依赖，与项目「可复现」目标一致。是否采用需 TBD-026 确认。

---

### 3.3 货箱物理挂载（V4 前置台阶）

#### 3.3.1 目标

把 V1 的「抓取以数据变化代替」（SRS D-026）与「GRASPING/PLACING 固定延时」（D-045）升级为**模型层面的可见变化**：货物在仿真里是一个实体，取货时挂到 AMR 上随车移动，放货时卸下。**不改协议、不改状态机、不改 `goods_status` 四量**。

#### 3.3.2 要求

| 项 | 要求 |
| --- | --- |
| 模型 | 在 `data/warehouse_01/` 或 `simulation_ws` 的 world 中增加货箱模型；`goods_id` 与模型的映射关系需可配置 |
| 挂载点 | AMR 上定义承载 link（或复用现有 `base_link`），挂/卸通过 Gazebo 的 attach/detach 能力实现（具体插件 TBD-028） |
| 触发时机 | 由 `GRASPING` / `PLACING` 状态推进时触发，仍可复用 V2 BT 的 `Grasp` / `Place` 占位节点 |
| 多机隔离 | 货箱实例必须带 `robot_id` / `instance_id` 维度，避免多机抢同一个模型（INV-11） |
| 与 V4 的关系 | V4 只把「触发挂载的那一下」换成机械臂抓取动作；**货箱模型、触发时机、状态语义在 V4 保持不变** |
| 载重影响 | 是否影响底盘加速度/惯量为可选项，由 TBD-028 决定；若启用需附调参记录 |
| 失败处理 | 挂载失败必须有明确上报（复用 `CARRY_ABORTED`），不得静默继续 |

#### 3.3.3 明确不做

- 不实现机械臂、夹爪、MoveIt、接触检测、抓取姿态规划（V4）。
- 不引入视觉识别（4 类可选项见 3.4，且需单独确认）。
- 不改变 D-026/D-045 在 SRS 中的**历史决策记录**，而是在新版本章节中说明「V3 起升级为模型变化」。

---

### 3.4 可选项（非必须，需单独确认后才可纳入本版）

| 可选项 | 价值 | 边界 |
| --- | --- | --- |
| AprilTag / 色块校验货物 | 用感知结果校验 `goods_id`，防止「搬错货」；亮点是**业务一致性校验**而非识别算法 | 需换深度/可用相机插件（`camera.urdf.xacro` 当前为普通 RGB box）；不实现识别算法研究 |
| rosbag2 任务回放 | 面试时可「倒带」讲解一次故障，ROS2 应用岗必备技能 | 需定义录制 topic 白名单与存储位置（TBD-033） |
| 自然语言任务入口 | 「把 A 区的红箱子搬到 B 区」→ LLM 解析成结构化 task → 走现有注入接口 | **LLM 只做意图解析**；所有数量、`robot_ids`、点位约束由 Edge 校验；解析失败回退表单。不得让模型直接做调度决策 |
| `/clock` 与 TF 时间同步实验 | 复用 `nav2-tuning-notes.md` 第 4 节「仍待观察的风险」，把隐患变成可演示实验 | 侵入性较高，建议先只做观测记录 |

---

## 4. 接口与数据模型变更汇总

| 类别 | 变更 | 位置 | 状态 |
| --- | --- | --- | --- |
| MQTT Topic | 启用 `warehouse/{warehouse_id}/task` / `task/status`（常量已存在） | `common/mqtt/mqtt_topic.h` | 预留 → 启用 |
| 连接 | Edge 新增 Cloud 侧 MQTT 客户端（建议方案） | `edge_server/src/` | TBD-027 |
| 消息 | Cloud → Edge 任务消息、Edge → Cloud 状态消息（结构已在契约 4.2/4.9 定义，需补齐幂等字段与断连重试规则） | `interface-contract.md` | 待补充 |
| HTTP | **保留全部 V1 路由，不新增为主**；Web 若需要聚合查询，新增只读接口需同步契约 | `edge_server/src/http/http_server.cpp` | 尽量不新增 |
| 数据表 | 任务来源标识（区分 HTTP 注入 / Cloud 下发）是否需要，待定 | `tbl_task` | 待定 |
| 仿真模型 | 货箱 SDF / world、承载 link、goods→模型映射配置 | `data/warehouse_01/`、`simulation_ws/`、`robot_ws/src/amr_description/urdf/` | TBD-028 |
| ROS2 接口 | **不变** | `amr_common/ros_interface_names.h` | 冻结 |

---

## 5. 文件级改动清单

| 类型 | 路径 | 说明 |
| --- | --- | --- |
| 新增 | `cloud_platform/`（当前仅 `.gitkeep`） | Cloud 任务入口实现，形态由 TBD-017 决定 |
| 新增 | `edge_server/src/communication/cloud_mqtt_client.*`（建议名，TBD-027） | 连接 Cloud 侧 EMQX、订阅任务 topic、转交 TaskService |
| 新增 | Web 前端目录（建议 `cloud_platform/web/` 或 `edge_server/web/`，TBD-026） | 静态看板页面 |
| 新增 | 货箱模型与映射配置 | `data/warehouse_01/`、`simulation_ws/src/warehouse_simulation/` |
| 新增 | `docs/architecture/adr/0003-cloud-task-ingress.md`（建议） | 记录双 EMQX 连接、兼容接口保留、TBD-017 关闭结果 |
| 修改 | `edge_server/src/service/task_service.cpp` | 接收 Cloud 订阅入口的任务，按 `task_id` 幂等创建 |
| 修改 | `edge_server/config/edge_server.yaml` | Cloud broker 连接配置；**凭据走环境变量，不写明文** |
| 修改 | `.env.example`、`edge_server/scripts/run_edge_server.sh` | 新增 Cloud 侧凭据占位（若需要） |
| 修改 | `interface-contract.md` 第 4 节 | 补齐 Cloud 链路的幂等、重试、断连规则 |
| 修改 | `docs/requirements/srs.md` 第 8.4 节、第 9 章 TBD-017 | **需项目所有者授权后修改** |
| 修改 | `README.md`、`docs/assets/architecture.svg` | 更新版本范围与架构图（Cloud 层由虚线变实线） |
| **禁止改动** | V1 HTTP 路由行为、`task_state` 枚举、`goods_status` 四量、ACK / 待补报、`amr_common` 常量语义 | 冻结 |

---

## 6. 文档同步清单

| 文档 | 动作 |
| --- | --- |
| `docs/requirements/srs.md` | 关闭 TBD-017；8.4 节标注 Web 看板本版范围；**需授权** |
| `docs/requirements/interface-contract.md` | 新增 Cloud↔Edge 链路的幂等/重试/断连规则；更新第 7 节 TBD |
| `docs/architecture/adr/0001-*.md` | 标注决策 2 已落地 |
| `docs/architecture/adr/README.md` | 新增 ADR-0003 索引 |
| `docs/architecture/system.md` | Cloud Platform 由规划变为已实现层；双 EMQX 连接示意 |
| `docs/architecture/simulation.md` | 货箱模型与 attach/detach 说明 |
| `docs/development/build-run.md` | 新增 Cloud / Web 启动步骤与凭据配置 |
| `README.md` / `docs/assets/architecture.svg` | 版本范围与架构图 |
| `docs/architecture/version-roadmap.md` | 本分册状态更新（完成后） |

---

## 7. 实施顺序建议

1. **先关闭 TBD-017 / TBD-027 / TBD-029 / TBD-026 / TBD-028**，产出 ADR-0003，按重大变更流程征得同意。
2. **打通 Cloud → Edge → AMR 链路**：实现 Cloud MQTT 客户端与幂等创建，先跑通一个任务，验证 V1 HTTP 注入仍可用。
3. **状态回传**：Edge 向 `task/status` 上报，验证断连恢复后 Cloud 能拿到最终状态。
4. **Web 看板**：先做三个必须页面，数据源全部用现成接口，不新增后端。
5. **货箱挂载**：加模型与 attach/detach，跑通「货物可见地随车移动」，验证多机不抢同一模型。
6. **可选项**：按确认结果决定是否纳入，不纳入则记录为更后版本。
7. **验证与 DoD**：全链路演示录制、文档同步、不变式复核。

---

## 8. 验收清单

- [ ] Cloud 下发的任务能端到端执行到 `CARRY_COMPLETED`，Edge 与 AMR 行为与 V1 HTTP 注入一致。
- [ ] 重复投递同一 `task_id` 不产生重复任务、不重复下发、不重复占用配额。
- [ ] Cloud 与 Edge 之间断连恢复后，任务最终状态可被正确获取。
- [ ] `POST /api/tasks`、`GET /api/tasks/{id}`、cancel 三个 V1 接口行为未变（回归）。
- [ ] Cloud 侧与 Edge 侧 EMQX 保持隔离，Cloud 不直接连接 AMR 侧 broker。
- [ ] `robot_ids` 语义按 TBD-029 结论执行；未放宽前仍要求非空且不自动选机。
- [ ] Web 三个必须页面可用，且除「下发/取消」外无其他写操作。
- [ ] 货箱在取货时挂到 AMR、随车移动、放货时卸下；多机场景下模型实例不冲突。
- [ ] 挂载失败有明确 `CARRY_ABORTED` 上报，不静默继续。
- [ ] 第 3 节全部跨版本不变式逐条复核未破坏。
- [ ] 全链路编译/启动通过；`build-run.md` 更新后他人可复现。
- [ ] 第 6 节文档同步完成；TBD-017 已关闭。
- [ ] 提交说明附「为什么这么实现」（双 EMQX 隔离、兼容接口保留、货箱作为 V4 台阶）。

---

## 9. 本版本 TBD

| 编号 | 问题 | 需要明确的具体内容 | 开工前必须明确 |
| --- | --- | --- | --- |
| TBD-017 | Cloud Platform 形态（引用 SRS） | CLI / HTTP 脚本 / FastAPI 三选一；任务创建参数输入方式 | 是 |
| TBD-027 | 双 EMQX 连接细节 | Cloud 侧 broker 地址、凭据注入、topic 映射、是否由 Edge 内 `cloud_mqtt_client` 桥接 | 是 |
| TBD-029 | 云端是否改变选机语义 | 是否仍要求 `robot_ids` 非空；是否允许云端/Edge 自动选择参与 AMR | 是 |
| TBD-026 | Web 技术栈与部署 | 前端框架、独立进程与否、静态资源托管、只读边界鉴权 | 否（设计阶段确定） |
| TBD-028 | 货箱模型与挂载 | 模型来源、attach/detach 插件、goods→模型映射、载重是否影响动力学 | 是 |
| TBD-006 | MQTT 安全（引用 SRS） | 最小可行认证与访问控制，Cloud 侧凭据同样不得入仓 | 是 |
| TBD-033 | 演示与证据归档（引用） | rosbag2 白名单、截图/录像存放路径 | 否 |

---

## 10. 为什么这么实现（取舍）

- **Cloud 与 Web 合并**：两者是同一条链的两端，拆版会产生「有后端没画面」；且按 ADR-0001，Cloud 的功能增量本就最小，不值得独占一版。
- **保留 V1 HTTP 注入**：ADR-0001 已明确；同时也保证 V1 的验收场景在 V3 之后仍可回归，避免「升级即破坏」。
- **双 EMQX 不合并**：INV-10 是已冻结的架构约束，Cloud 不直接接触 AMR 侧 broker，隔离边界清晰，也是面试里能讲的安全设计。
- **Web 只做观察窗**：SRS 8.4 的完整 WMS / MySQL / Redis / 统计平台一旦开做就会范围失控；只读看板用现成接口即可让演示有画面，投入产出比最高。
- **货箱挂载先于机械臂**：把「货物物理存在」这个变量提前消化，V4 只替换「谁把它拿起来」，是控制单次版本风险的关键。
- **自然语言入口严格限定在意图解析**：既蹭到「大模型 + 机器人」的话题，又用 Edge 校验兜住可靠性，避免被追问「模型输出不可靠怎么办」时答不上来。

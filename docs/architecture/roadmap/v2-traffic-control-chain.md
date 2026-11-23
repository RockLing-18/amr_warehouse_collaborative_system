# V2 交通路权控制链：进程边界、数据链路与时序

> **⚠️ 2026-11-22 V0.20 定案变更**：本版按项目所有者 8 项拍板，改为「**4 点凸 `polygon` 通行控制区 + AMR 中心点 `point_in_poly` 进出边沿判据 + 无 `plan` 订阅**」。旧「有向触发段 / 外移释放线 / `crossed()` 主判据 / `NARROW` / `TRAFFIC_WAIT` / `side`」口径**已废弃**，一律以 `.workbuddy/artifacts/traffic-gate-8-decisions-frozen.md` 为准；下文保留的对比语境均显式标注「已废弃 / 历史」。

> **状态：架构说明 / 已定案**，2026-11-20 第六轮新增、第七轮（项目所有者拍板）更新，2026-11-22 V0.20 定案同步。
> **本文件不新增设计决策。** 决策权威仍是 `v2-resource-arbitration-and-bt.md` 3.2 与 `../version-roadmap.md` 的 **INV-13 / INV-14**；本文件只解决它们**没有写清楚**的那一层：**谁连谁、走哪条 topic、谁调谁、按什么顺序调**。
> **第七轮已落盘的定案**：① 路权逻辑 = `amr_behavior_manager` 包内 `TrafficGate`，**不设独立模块**（已废弃的独立模块命名已全仓清理）；② 专用 ROS2 topic = `traffic_request` / `traffic_response`；③ MQTT = `amr/{robot_id}/traffic_rights/request` / `response`，动作由 payload `op` 承载、不分层；④ V2 需求已写入 `srs.md`（第七轮时为 V0.19，**现为 V0.20**）。
> **实现者读法**：先读本文 §0〈一页速读〉与 §4〈逐跳表〉，再读 [V2 实现规格](v2-implementation-spec.md)（消息字段 / 状态机 / 伪代码 / 全场景流程），最后回 V2 分册 3.2 查决策依据。凡标「事实」的以源码为准；凡标「TBD」的不得自行选择。
> 配套图：`../../assets/v2-traffic-dataflow.svg`、`../../assets/v2-traffic-sequence.svg`（人类阅读版）；**纯文字 / agent 可读版见 [`v2-traffic-diagrams.md`](v2-traffic-diagrams.md)**，用 mermaid 转写同一批图，语义仍以本文 §4 / §6 为准。配套实现级文档：`v2-implementation-spec.md`。

## 章节速查

| 章节 | 讲什么 | 回答谁的疑问 |
| --- | --- | --- |
| §0 一页速读 | 整条链一句话 + 三条红线 | 第一次读的人 |
| §1 V2 现在定了多少（第七轮更新） | 五层确定度 | 「需求/流程/数据链路全定了吗」 |
| §2 四处断点与事实错误 | 第六轮实证，第七轮**已全部消除** | 「别的 agent 能理解吗」 |
| §3 已定案架构（INV-14） | 进程归属与理由 | 路权逻辑到底放哪 |
| §4 数据链路逐跳表 | 13 跳接口、**已定名 topic**、方向、V1 现状 | 「流程不是很清楚」 |
| — 实现规格 | 另见 `v2-implementation-spec.md`：消息字段、状态机、伪代码、全场景流程 | 「好让 agent 实现代码」 |
| §5 取消导航完整调用链 | 复用现成接口，不新增 | 「如取消导航」 |
| §6 五个时序场景 | 正常 / 被拒 / 释放 / 改道 / 兜底 | 「有流程图了吗」 |
| §7 消息与常量清单 | V1 已有 vs V2 新增 | 开工前核对 |
| §8 实现者必读检查项 | 10 条可勾选 | 防止做错 |
| §9 仍未冻结（已剔除已定案项） | 不许自行选的项 | 边界 |
| §10 文档关系与引用约定 | 编号怎么写 | 防混淆 |

---

## 0. 一页速读

**一句话（已定案）**：V2 路权逻辑是 **`amr_behavior_manager` 包内的 `TrafficGate`（类 + BT 节点）**，**不存在独立模块 / ROS2 包**；`amr_agent` 只做 **MQTT ↔ ROS2 的透明桥**（`TrafficBridge`，走 `traffic_request` / `traffic_response`，**不改** `TaskCommand` / `TaskEvent`）；Edge 侧 `traffic_rights_service` + `ResourceBroker` 持有全局 lease 与 FIFO 队列。位姿从 **导航 Action 的 feedback** 的 `current_pose`（AMR 中心点）来，几何从**独立 `traffic_gates.yaml` 的 4 点 `polygon` + `wait_poses`** 来，**不订阅 `plan`**；主判据为中心点 `point_in_poly` 的 `inside` 进出边沿，「取消导航」直接调**现成的** `NavigationClient::cancelNavigation()`。

**最小链路（读图前先记这一条）**：

```text
action feedback.current_pose（中心点） / traffic_gates.yaml（4 点 polygon + wait_poses）
   -> [amr_behavior_manager] TrafficGate（中心点进出边沿判定 + 决策；无 plan 订阅）
   -> traffic_request（ROS2 相对 topic，已定名，V2 新增）
   -> [amr_agent] TrafficBridge（透明桥，V2 补实现）
   -> MQTT amr/{robot_id}/traffic_rights/request  (payload.op = REQUEST|CANCEL|RELEASE)
   -> [Edge] AmrCommunication（订阅 amr/+/traffic_rights/request）-> traffic_rights_service
        -> ResourceBroker（FIFO / lease / lease_ttl）-> SQLite
   -> MQTT amr/{robot_id}/traffic_rights/response（单播，已定名）
   -> TrafficBridge -> traffic_response（ROS2 相对 topic，已定名，V2 新增）
   -> TrafficGate
        ├─ GRANTED（REQUESTING/S1）-> 【零动作】，车继续 NavigateToPose(D)
        └─ REJECTED -> NavigationClient::cancelNavigation()
                       + 等 result 回调 m_goal_active=false
                       + navigateToPose(同侧就近等待点) + 固定 10 s 重发
                       + 仅 WAITING 收到 GRANTED 立即 navigateToPose(D)
```

**三条红线（做错就返工）**：

1. **`amr_agent` 不碰位姿、不判几何、不做业务决策**。它是唯一 MQTT 网关，只负责把 `traffic_request` 原样发成 MQTT、把 MQTT 响应原样转成 `traffic_response`。
2. **`amr_behavior_manager` 不直连 MQTT**，不新增第二个 MQTT client。否则出现两个心跳源、两套断网待补报，违反 V2 分册 3.0.1。
3. **Edge 不知道 Nav2 存在**。Edge 只认 `resource_id` / `request_id` / `epoch` / `lease_ttl`，不下发坐标、不下发途经点、不感知导航目标。

---

## 1. V2 现在到底定了多少（如实分层）

| 层 | 状态 | 说明 |
| --- | --- | --- |
| ① 需求与版本边界 | **已冻结并进 SRS** | `../version-roadmap.md` + V2 分册第 1/2 节 + **`srs.md` V0.20 第 4.10 / 4.11 节**（2026-11-20 第七轮授权写入 V0.19，2026-11-22 升 V0.20）。 |
| ② 关键设计取舍 | **已冻结** | 运行时按需触发主模型、**4 点通行控制区 + AMR 中心点 `point_in_poly` 进出边沿判据 + 无 `plan` 订阅**（V0.20 定案）、目标全程 D、固定 10 s、Edge 中心 FIFO + TTL、电量 B/D、充电 B、BT C 方案、INV-13。见 V2 分册 3.2 / 3.3 / 3.4。 |
| ③ 业务流程与状态机 | **主流程已定，前置阻塞未清** | 主流程（申请 / 授权 / 被拒 / 等待 / 重发 / 释放）与 lease 状态机（`PENDING → GRANTED → HELD → RELEASED`）已定；但 **`amr_navigation` 的 4 个缺陷是阻塞项**（V2 分册 3.2.3），且**跨模块调用时序此前从未画过**，本文件 §6 与配套时序图补上。 |
| ④ **数据链路（谁连谁）** | **已定案** | 第七轮：模块归属（INV-14）、ROS2 / MQTT topic 命名、payload `op` 形态、消息字段全部冻结；§2 的 4 处断点已全部消除。 |
| ⑤ 参数与接口形态 | **命名已定，数值仍开** | 仍待：TBD-020（`queue_seq` / `PENDING` 超时 / `lease_ttl` 默认值 / 表最终字段）、021（`polygon` / `wait_poses` 字段名、`L_pull` / `min_release_clearance` 净距数值）、023 / 024（BT 与急停）、038（滞回双阈值 / 去抖）、039（AMR 侧 `traffic_gate_loader` 读取 `traffic_gates.yaml`）。见 §9。 |
| ⑥ 代码实现 | **基本为零** | `traffic_rights_service`、`TrafficBridge`、`TrafficRequest/Response` 消息与编解码、`tbl_resource_lease`、FIFO、`TrafficGate` / BT 节点 **全部不存在**。V1 只留下 2 个 topic 常量、两侧被注释的 handler 注册、一个空壳前向声明。 |

**结论（第七轮）**：需求、取舍、业务流程、数据链路、接口命名**均已确定**，且 V2 需求已进 SRS；尚未确定的只剩**数值参数与少量技术选型**（§9），以及代码实现本身。别的 agent 现在可照 [V2 实现规格](v2-implementation-spec.md) 开工。

> 顺带回答「`amr_power_manager` 应该不复杂」：**判断正确**。它是单订阅（`power_mode`）+ 定时积分 + 单发布（`power_level`），零判定、零 `cmd_vel` 订阅；`robot_status → amr_agent → Edge` 的上报链路 V1 已通，V2 只需移除两处 `100.0` 硬编码（V2 分册 3.3.1 / 3.3.2.3）。它**不需要跨进程控制链**，这也是它和 V2 路权逻辑（`TrafficGate`）复杂度差一个量级的原因。

---

## 2. 四处断点与事实错误（第六轮源码实证 · 第七轮已全部消除）

> 以下均为**只读勘查**结果，未改任何源码。证据路径相对工作区根目录。
> **第七轮状态**：四项的**事实部分保留**（仍是实现者必读的源码真相），但「断点」均已关闭 —— ① 模块归属由 INV-14 定案、② 桥 topic 已定名、③ 事实错误已纠正、④ 数据源已在实现规格 §4 写成可照抄的代码路径。

### 2.1 形态两处建议互相打架【已解决：INV-14 定案】

| 位置 | 原文要点 |
| --- | --- |
| V2 分册 3.0 归属表 | 「建议并入 **`amr_agent` 通信层** + BT 条件节点，不新建独立进程」 |
| V2 分册 3.0.1 | 「建议实现为 **`amr_behavior_manager` 内的一组 BT 节点** + 复用 `amr_agent` 已有 MQTT 通道」 |

两处都标「形态待定」，但**给出了两个不同的落点**。实现者二选一，会做出两套完全不同的架构：放进 `amr_agent` 就得跨进程拿位姿、跨进程触发 cancel；放进 `amr_behavior_manager` 则是进程内调用。

> **✅ 第七轮定案**：采用 `amr_behavior_manager` 内 `TrafficGate`（§3）；两处「形态待定 / 建议」表述已从 V2 分册删除，全仓不再使用已废弃的独立模块名。实现者**不得再二选一**。

### 2.2 桥通道缺失，且被 3.4.5 明文禁止复用【断点】

- V2 分册 3.4.5 写：「`amr_agent` 与 `amr_behavior_manager` 继续通过 ROS2 Topic `task_command` / `task_event` 通信。**V2 不新增、不修改这两个消息结构**」。
- 但路权请求 / 响应必须在这两个进程之间往返；第六轮时文档**没有给出这对新 ROS2 topic 的名称与形态**，链路在中间断开。**✅ 第七轮已定名**：`traffic_request` / `traffic_response`，消息字段见实现规格 §2。
- 源码事实：`task_command` / `task_event` 是 `amr_interfaces/msg/TaskCommand.msg` 与 `TaskEvent.msg`，由 `amr_agent/src/bridge/task_bridge.cpp` 双向桥接；`TaskBridge` 已用 `registerExternalHandler()` + `setExternalSubscribe()` 注册 4 个 MQTT handler —— **这个范式可以直接复制给 `TrafficBridge`**，不需要新机制。

### 2.3 「V1 已有 response 订阅」是事实错误【误导】

V2 分册 3.2.2（约 304 行）写「V1 已有 `amr/traffic_rights/response/{robot_id}` 订阅，Edge 主动下发」。**源码里没有这个订阅**：

| 实际存在的东西 | 证据 |
| --- | --- |
| 2 个 topic 字符串常量 | `common/mqtt/mqtt_topic.h:53-54`：`TRAFFIC_RIGHTS_REQ` / `TRAFFIC_RIGHTS_RSP_PREFIX`；无对应 `make*Topic()` helper |
| Edge 侧被注释的 handler 注册 | `edge_server/src/communication/amr_communication.cpp:110-115`（`setSubscribe()` 的 5 条订阅里**没有**路权 topic） |
| AMR 侧被注释的 handler 注册 | `robot_ws/src/amr_agent/src/communication/edge_communication.cpp:152-157`；`edge_communication.h:48-51` 三行 callback 全注释 |
| 空壳 | `amr_agent/src/amr_agent_node.h:18` `class TrafficBridge;` 前向声明 + `:38` 成员指针；**无定义、无实例化、无 `traffic_bridge.cpp`** |
| 文档自认 | `docs/development/project-review.md:54` 已记录「订阅整段注释，`TrafficBridge` 为空壳」 |

即：**`response/{robot_id}` 订阅是 V2 要新增的，不是「启用预留接口」**。该句已在本轮同步为事实描述（仅纠错，未改任何定案）。

### 2.4 位姿数据源表述有歧义（V0.20 定案：无 `plan` 订阅，仅 action feedback）【断点 · 已关闭】

| 文档写法（V0.20 口径） | 源码事实 |
| --- | --- |
| 「只读 `amr_navigate_to_pose` action feedback 的 `current_pose`（AMR 中心点）；**不订阅 `plan`**」 | `current_pose` **不是独立 ROS2 topic**，是 `amr_navigate_to_pose` **Action 的 feedback 字段**（`amr_interfaces/action/AmrNavigateToPose.action:21`），由 `navigation_manager.cpp:87-116` → `navigation_server.cpp:419-438` `goal_handle->publish_feedback()` 上抛，`navigation_client.cpp:62-65` 转发给 behavior manager |
| 同上 | **`amr_behavior_manager_node.cpp:125-129` 的 feedback 回调是空实现**（`(void)feedback;`）——目前**没有任何上层消费者**，V2 必须把它填上，接入中心点 `point_in_poly` 进出边沿判定 |
| ~~「V2 要新增订阅 `plan`」~~（旧口径，**已废弃**） | **`plan` 在全项目没有任何 ROS2 订阅代码**，只有 `amr_navigation/rviz/nav2_view.rviz:296-303` 的 RViz 显示配置。**V0.20 定案省掉整个 `plan` 订阅**：`onPlan()` / `onPlanChanged()` / `pathIntersects*` / `D_trigger` / 改道去抖变量全部移除，提前量由 4 点通行控制区侧边拉长 `L_pull` 静态提供。相对名 `plan` 仅 RViz 显示用，路权链不订阅 |
| （隐含可用） | `robot_pose` topic 为 **1 Hz**（`navigation_server.cpp:143-147`，TF `map → <ns>_base_footprint`）。**禁止作主判据**：1 Hz + `v_max = 0.26 m/s` 意味着相邻采样间隔内可位移约 0.26 m，足以漏判进出区边沿；仅作初始位姿 / 兜底。主判据数据源是 action feedback 的 `current_pose` |

另注：V1 已有 `MqttMessageRouter` 支持 `+` / `#` 通配（`amr_agent/src/mqtt/mqtt_message_router.cpp:104-141`），Edge 侧 `setSubscribe()` 已在用 `amr/+/status` 等通配订阅 —— 所以「Edge 用通配符订阅路权」是**既有范式的平移**，不是新架构。

---

## 3. 已定案架构（INV-14，2026-11-20 第七轮）

> 以下是**唯一自洽且改动最小**的形态，项目所有者已拍板：**路权逻辑放 `amr_behavior_manager` 内 `TrafficGate`，不新建独立模块 / ROS2 包；`amr_agent` 只补 `TrafficBridge` 透明桥**。topic 命名见 §3.3 与 [V2 实现规格 §1](v2-implementation-spec.md)。实现者**不得再选择其它落点**。

### 3.1 三进程 + Edge 的职责切分

| 模块 | 侧 | 职责 | **不做** |
| --- | --- | --- | --- |
| `TrafficGate`（**V2 路权逻辑本体**，`amr_behavior_manager` 包内的类 + BT 节点，**不设独立模块**） | AMR | 消费 action feedback 的 `current_pose`（中心点）+ 加载独立 `traffic_gates.yaml` 的 4 点 `polygon` / `wait_poses`（`MapInfo.traffic_gates_yaml` 下发） → `point_in_poly` 中心点进出边沿（滞回双阈值 + 状态过滤）→ 边沿时刻算 `entry_side` / 就近等待点 → 生成 `TrafficRequest`（`request_id` / `resource_id` / `ETA` / `lease_ttl` / `epoch`）→ 处理 response（S1 零动作 / S2 立即重发 D）→ **调用 `NavigationClient`** → 四触发释放。**不订阅 `plan`** | 不直连 MQTT、不做全局排序、不改 `task_state` |
| `TrafficBridge`（补 `amr_agent` 内已有的空壳） | AMR | `traffic_request` ↔ MQTT request；MQTT `response/{robot_id}` ↔ `traffic_response`；沿用 `TaskBridge` 的 `registerExternalHandler` / `setExternalSubscribe` 范式；沿用 `PendingReport` / ACK 幂等范式 | 不判几何、不订阅位姿、不订阅 `plan`、不调导航 |
| `navigation_client` / `amr_navigation` / Nav2 | AMR | 保持 V1 对外接口不变：`amr_navigate_to_pose` action（goal / feedback / result / cancel）+ `robot_pose`；`plan` 仅 RViz 显示配置，**路权链不订阅** | 不新增 ROS2 导航接口（V2 分册第 4 节已冻结） |
| `AmrCommunication` → `traffic_rights_service` → `ResourceBroker` → SQLite | Edge | 通配订阅 `amr/+/traffic_rights/+`；按动作分层或 payload `op` 分发；FIFO `queue_seq`、`request_id` 幂等、`epoch` 防陈旧、lease 原子授权、`lease_ttl` 到期回收；单播 response | 不感知 Nav2、不下发坐标、不广播占用状态 |

### 3.2 为什么路权逻辑必须放在 `amr_behavior_manager`（而不是 `amr_agent`）

三个数据 / 控制点全在这个进程里，**放进 `amr_agent` 会凭空多出三条跨进程链路**：

| 需要的输入 / 动作 | 现在在哪 | 若放 `amr_agent` 的代价 |
| --- | --- | --- |
| `current_pose`（action feedback） | `NavigationClient` 的 feedback 回调（`amr_behavior_manager`） | 得把 feedback 经新 topic 转发给 agent，或让 agent 也去订阅 action feedback（不可行，feedback 只对 goal 订阅者可见） |
| 4 点 `polygon` / `wait_poses` 几何（`traffic_gates.yaml`）、`current_pose`（action feedback） | `traffic_gate_loader` + `GoodsPointResolver`（`amr_behavior_manager`）；**无 `plan` 订阅** | 得把地图几何再下发一份给 agent，形成第二份真值 |
| **取消导航 / 重发目标** | `NavigationClient::cancelNavigation()` / `navigateToPose()`（`amr_behavior_manager`） | 得新增一条「agent 请求 behavior 取消导航」的 ROS2 接口，并定义其应答与竞态 |

而放在 `amr_behavior_manager` 内，**取消导航是进程内一次函数调用**（§5），零新增 ROS2 接口。**第七轮已按此定案（INV-14）**；3.0 归属表那句「并入 `amr_agent` 通信层」现统一理解为「**复用 `amr_agent` 的 MQTT 通道**」，不是「逻辑搬进 agent」，且该表已改写为定案表述。

### 3.3 为什么 `amr_agent` 只做透明桥

- V1 已确立「`amr_agent` = 唯一 MQTT 网关」（`main.cpp` + `EdgeCommunication`），第二个 MQTT client 会带来第二个心跳源、第二套 Last Will、第二套断网待补报。
- 已有 `class TrafficBridge;` 空壳前向声明（`amr_agent_node.h:18,38`）—— **V1 作者本来就是这么设计的**，V2 只是补上实现，不是发明新架构。
- `TaskBridge`（`bridge/task_bridge.cpp:33-91`）已经示范了完整范式：内部 ROS2 publisher/subscriber + 4 个 `registerExternalHandler` + 4 个 `setExternalSubscribe` + `PendingReport` 定时器。照此复制即可。
- **不改 `TaskCommand` / `TaskEvent`**（V2 分册 3.4.5 红线），改用**已定名**的一对专用 topic：`traffic_request`（`amr_common::topic::TRAFFIC_REQUEST`） / `traffic_response`（`amr_common::topic::TRAFFIC_RESPONSE`），消息 `TrafficRequest.msg` / `TrafficResponse.msg`（字段见实现规格 §2）。

### 3.4 为什么 Edge 不知道 Nav2

Edge 的输入只有 `resource_id`（区域 id，不是坐标）、`ETA`、`lease_ttl`、`epoch`；输出只有 `GRANTED` / `REJECTED` / `PENDING`。它既不下发等待点也不下发途经点，因此 Nav2 的动态选路完整保留（INV-13）。这是「不写死路线」在链路层的体现。

---

## 4. 数据链路逐跳表（**核心，实现者照此连线**）

> **本表是语义真值。** 同一批连线的结构视图见 [`v2-traffic-diagrams.md` §1](v2-traffic-diagrams.md) 的 mermaid `flowchart`（图中只标 J 编号，精确接口回查本表）；人类阅读版见 `../../assets/v2-traffic-dataflow.svg`。三者等价，冲突时**以本表为准**。

> 「V1 现状」列里的 **新增** = V2 要写的代码；**已有** = 直接复用；**待修** = 阻塞缺陷。所有 ROS2 名称一律**相对名**，由 launch namespace 解析为 `/{robot_id}/...`，**禁止写绝对名**（`docs/requirements/interface-contract.md` 第 5 节）。

| # | 从 | 到 | 接口 / 名称 | 方向 | V1 现状 | V2 动作 |
| --- | --- | --- | --- | --- | --- | --- |
| J1 | ~~Nav2 `bt_navigator`~~ | `TrafficGate` | ~~ROS2 topic `plan`~~ → **无订阅**（V0.20 定案） | — | 仅 RViz 显示 `plan`，**无代码订阅** | **不新增订阅**：`onPlan()` / `onPlanChanged()` / `pathIntersects*` / `D_trigger` / 改道去抖变量全部省掉；提前量由 `L_pull` 拉长控制区静态提供，进出判定只用 J2 的 action feedback |
| J2 | `amr_navigation` | `NavigationClient` → `TrafficGate` | action `amr_navigate_to_pose` 的 **feedback `current_pose`（AMR 中心点）** | 只读 | 已上抛，但 `amr_behavior_manager_node.cpp:125-129` **回调为空** | **填实空回调**，把位姿推给 `TrafficGate` 做 `point_in_poly` 进出边沿判定；**不新增 topic、不订阅 `plan`** |
| J3 | 独立 `traffic_gates.yaml`（`MapInfo.traffic_gates_yaml` 下发，第 4 文件 / 8 处链路） | `traffic_gate_loader` → `TrafficGate` | `resource_id` + 4 点凸 `polygon` 通行控制区 + `wait_poses{l1,l2}`（**无 `side` / `forward` / `trigger` / `release`**） | 加载 | V1 loader 只读 `id/name/type/goods_ids`，**无窄道几何**；`zones.yaml` / `stations.yaml` 不动 | **新建 `traffic_gate_loader`**（TBD-039，V1 `ZoneStationLoader` 不改）；`polygon` 既作中心点进出主判据，也供 validator / 异常裁决 / 可视化 |
| J4 | `TrafficGate` | `TrafficBridge` | ROS2 topic `traffic_request`（**已定名**，`TrafficRequest.msg`） | 出站 | 不存在 | **新增** msg + pub/sub；`op` / `request_id` / `resource_id` / `robot_id` / `epoch` / `eta_sec` / `lease_ttl_sec` |
| J5 | `TrafficBridge` | MQTT Broker | `amr/{robot_id}/traffic_rights/request`（**已定名**，单一上行 topic，payload `op` 三态） | 出站 | V1 常量无 `robot_id` 层，**handler 订阅被注释** | **替换常量 + 注册 handler**；QoS 1；`request_id` 幂等 |
| J6 | MQTT Broker | `AmrCommunication`（Edge） | 通配 `amr/+/traffic_rights/request`（**已定名**，只订上行） | 入站 | 通配能力已有（`MqttMessageRouter`），**未订阅路权** | **新增**订阅；按 payload `op` 分发；`robot_id` 与 topic 交叉校验 |
| J7 | `traffic_rights_service` | `ResourceBroker` → SQLite `tbl_resource_lease` | 进程内调用 / SQL | 内部 | 不存在 | **新建**；FIFO `queue_seq`、部分唯一索引仅覆盖 `GRANTED`/`HELD`、`PENDING` 可共存 |
| J8 | Edge | MQTT Broker | `amr/{robot_id}/traffic_rights/response`（**已定名，单播不广播**） | 出站 | 常量已有（形态不符），**无发布代码** | **新增**；`GRANTED` / `REJECTED` / `PENDING` / `CANCELLED` / `RELEASED` / `EXPIRED` + `request_id` + `epoch` |
| J9 | MQTT Broker | `TrafficBridge` → `TrafficGate` | ROS2 topic `traffic_response`（**已定名**，`TrafficResponse.msg`） | 入站 | **不存在**（含 §2.3 的事实错误） | **新增**订阅 `makeAmrTrafficResponseTopic(robot_id)` + 内部 topic |
| J10 | `TrafficGate` | `NavigationClient::cancelNavigation()` | **进程内函数调用** | 控制 | **已实现且已在用**（`handleTaskAssign` / `handleTaskCancel`） | **直接复用，不新增接口**；见 §5 |
| J11 | `TrafficGate` | `NavigationClient::navigateToPose(等待点 或 D)` | **进程内函数调用** | 控制 | 已实现 | 直接复用；目标全程为 D，等待点是唯一例外 |
| J12 | `TrafficGate` | 复用 J4→J5→J6→J7 | `op = CANCEL` / `op = RELEASE`（同一 `traffic_request` topic） | 出站 | 不存在 | 显式取消 / 四触发释放；排队中取消=出队，已授权取消=释放 lease |
| J13 | `ResourceBroker` 定时器 | SQLite | `lease_expires_at` 到期 → `EXPIRED` | 内部 | 不存在 | **新增**；AMR 崩溃 / `cancel` 丢包 / 车停在区内不动 / 位姿断流的最终兜底（改道驶离由「出区边沿」释放，停住 / 断流由本跳 TTL 承接） |

**附：`amr_power_manager` 的链路（对比用，确实简单）**

```text
amr_behavior_manager --power_mode(PowerMode)--> amr_power_manager --power_level(BatteryState)--> BT BatteryOK
                                                      |
                                                      +--> RobotStatus.battery --> amr_agent（已有订阅）--> Edge 只读镜像
```

一个订阅、一个积分定时器、一个发布；不改 `TaskCommand` / `TaskEvent`、不加 `amr_agent` 订阅、不跨进程控制。详见 V2 分册 3.3.2.3。

---

## 5. 取消导航的完整调用链（你最关心的那块）

**结论：不需要新增任何导航接口，现成链路可直接复用。** 这也是把 `TrafficGate` 放在 `amr_behavior_manager` 内的决定性理由。

```text
TrafficGate（被拒去等待点 / 改道或任务结束要释放资源 / 任务取消）
  │ 进程内调用
  ▼
NavigationClient::cancelNavigation()          robot_ws/src/amr_behavior_manager/src/navigation_client.cpp:81-102
  │  取 m_current_goal_handle，状态为 ACCEPTED/EXECUTING 时
  ▼
rclcpp_action::async_cancel_goal()            action 名 amr_navigate_to_pose（相对名）
  ▼
NavigationServer::handleCancel()              robot_ws/src/amr_navigation/src/navigation_server.cpp:310-318
  │  CancelResponse::ACCEPT
  ▼
NavigationManager::cancelNavigation()         robot_ws/src/amr_navigation/src/navigation_manager.cpp:152-160
  │  m_state = CANCELING
  ▼
async_cancel_goal() on Nav2 navigate_to_pose  （Nav2 原生 action）
  ▼
result CANCELED -> goal_handle->canceled(result)  navigation_server.cpp:398-402
  ▼
NavigationClient result 回调 -> m_goal_active = false
  │
  ▼ 【必须等这一步】
TrafficGate -> NavigationClient::navigateToPose(等待点) 或 (D)
```

**三个实现者必踩的坑**：

1. **`m_goal_active` 忙则拒绝**（`navigation_client.cpp:28-32`）。`cancelNavigation()` 是异步的，**必须等 result 回调把 `m_goal_active` 置回 false 之后**才能发等待点或 D；立即重发会被直接丢弃。建议 `TrafficGate` 用显式 `CANCELING → WAITING` 状态等待回调，不要靠 `sleep`。
2. **supersede 叠加**：`navigation_server.cpp:307` 对所有 goal 无条件 `ACCEPT_AND_EXECUTE`，busy 时（`:343-356`）内部再 cancel 旧 goal 重发。`TrafficGate` 的显式 cancel 与这条内部 supersede 可能叠加成两次 cancel。这是 4 个阻塞缺陷之一，**必须先修**。
3. **epoch 竞态**：`m_nav_epoch` 非 atomic、写于结果回调之后（`:344` / `:372` / `:442`），叠加 60 s 超时只 abort 不 cancel Nav2 goal（`:456-470`），可能让「cancel → 重发」白等 60 s。同样属前置阻塞项。

**四个前置阻塞缺陷**（V2 分册 3.2.3 已列为阻塞项，此处给源码坐标）：

| 缺陷 | 坐标 |
| --- | --- |
| 无条件 `ACCEPT_AND_EXECUTE` + supersede | `navigation_server.cpp:290-308`、`:343-356` |
| 60 s 结果超时只 abort、不 cancel Nav2 goal | `navigation_server.cpp:456-470`、`:381-389` |
| `m_nav_epoch` 无锁竞态（非 atomic） | `navigation_server.cpp:344` / `:372` / `:442` / `:831` |
| `accepted` 语义混淆（`async_send_goal` 后立即置 true） | `navigation_manager.cpp:58-65`、`:68-85` |

---

## 6. 五个时序场景（配套 `../../assets/v2-traffic-sequence.svg`）

> **本节编号步骤是语义真值。** 结构视图见 [`v2-traffic-diagrams.md` §2](v2-traffic-diagrams.md) 的四张 mermaid `sequenceDiagram`（S5 因触发并行保持表格）；人类阅读版见 `../../assets/v2-traffic-sequence.svg`。

> 五条泳道：**Nav2 / `amr_behavior_manager`(TrafficGate) / `amr_agent`(TrafficBridge) / Edge(Broker) / SQLite**。所有场景中 `task_state` 全程不变（INV-13）。

### 6.1 正常路径（S1）：进区边沿申请 → 授权 → 零动作（最常见）

1. `NavigateToPose(D)` 已发出；`TrafficGate` 消费 J2 的 action feedback `current_pose`（AMR 中心点）。**无 `plan` 订阅**。
2. 中心点 `point_in_poly(center, polygon)` 的 `inside` **false→true**（进 4 点通行控制区边沿，滞回确认）→ 边沿时刻算 `entry_side` 并锁存 → J4/J5/J6 发申请（带 `ETA` + `lease_ttl`，**不动 goal**）→ `IDLE → REQUESTING`。`L_pull` 侧边拉长保证申请早于车到真实窄道口。
3. Edge FIFO 授权，J8/J9 单播 `GRANTED`。
4. **`REQUESTING` 收 `GRANTED` → 零动作**（S1）：不减速、不 cancel、不重发 goal。车沿原 `NavigateToPose(D)` 直接通过；`REQUESTING → HELD`。
5. `GRANTED → HELD` 写入 SQLite；`lease_expires_at` 开始计时。

### 6.2 异常路径（S2）：被拒 → 同侧就近等待点 → 固定 10 s 重发 → 立即重发 D

1. Edge 返回 `REJECTED`（或 `PENDING` 未获批）。
2. `TrafficGate` → J10 `cancelNavigation()` → **等 result 回调 `m_goal_active=false`**（§5 坑 1）再发新 goal。
3. → J11 `navigateToPose(同侧就近等待点)`（`argmin dist` 于 `wait_poses[entry_side]`，`entry_side` 用进区边沿锁存值）；等待点**必须在入口端边外侧（上游）**（V2 分册 3.2.1.3 硬约束 1），否则车会无权限穿越窄道。
4. 停稳进入 `WAITING`：**Nav2 无活动 goal**（本就**无 `plan` 订阅**）、`cmd_vel` 闸门归零（模型 A，规避 `SimpleProgressChecker` 20 s 超时）。
5. 每 **固定 `T_retry = 10 s`（无 jitter）** 用**同一 `request_id`** 幂等重发；Edge FIFO + 幂等去重吸收多车同步申请。
6. **仅 `WAITING` 收 `GRANTED` 立即**（不等轮询）→ J11 `navigateToPose(D)` → `RESUMING → HELD`；任务目标恢复为 D，不改 `task_state`。
7. 等待期间若任务被取消 / 放弃等待 → J12 显式 `CANCEL`（排队中出队）。

### 6.3 正常释放（S3）：中心点出区边沿

1. `TrafficGate` 消费 J2 的 `feedback->current_pose`，`point_in_poly` 的 `inside` **true→false**（出 4 点通行控制区边沿，滞回确认）。
2. 立即 J12 发 `op=RELEASE` → Edge `HELD → RELEASED`；**车不停、不改 goal**。方向无关 → 掉头从入口退出、改道驶离控制区同样由该边沿覆盖。
3. 端边外移 `min_release_clearance` 保证「中心出区 ⇒ 车尾已出窄道」；不做逐帧 `footprint` 判定、不用 `crossed()` 主判据。

### 6.4 改道释放（S4）：保留语义、移除独立机制

1. 授权后重发 D，Nav2 改选绕行路（**无 `plan` 订阅、不检测 `plan` 相交**）。
2. 车驶离通行控制区 → **6.3 的中心点出区边沿** → J12 `op=RELEASE`；车停在区内不动 / 位姿断流 → **6.5 / J13 的 `lease_ttl` 到期 `EXPIRED`** 回收。端边不压分叉点，改道必然走出区域；S6「走绕行路零申请」的判据是「**从不进区**」。
3. 「改道即释放」是上述语义，**不设 `onPlanChanged()` / `pathIntersects*` / `plan` 相交去抖 / 位姿远离 / 改道冷却等独立机制**；`zonePolygon()` 保留供 validator / 异常裁决 / 可视化，只是不再被改道检测调用。四触发取最早保证权限不被占死、对向车不白等。

### 6.5 兜底（S5）：`SUCCEEDED` / `lease_ttl` / 显式取消

| 触发 | 链路 | 说明 |
| --- | --- | --- |
| 导航 `SUCCEEDED` / 任务结束 / 异常 `ABORTED` | `TrafficGate` → J12 release | 中心点出区边沿漏检时的兜底；**已降级为兜底，不是主判据** |
| `lease_expires_at` 到期 | Edge 定时器 → `EXPIRED` → 自动回收 | AMR 崩溃 / `cancel` 丢包 / 通信中断 / 车停在区内不动 / 位姿断流（不出区边沿）；**「怎么知道 AMR 取消了」的最终答案：不必知道** |
| AMR 显式 `CANCEL` | J12 → J6/J7 | `PENDING` → `CANCELLED` 出队；`GRANTED`/`HELD` → `RELEASED` + 配额回退；带旧 `epoch` 的消息一律丢弃 |

---

## 7. 消息与常量清单

| 名称 | 类型 | V1 | V2 |
| --- | --- | --- | --- |
| `amr_navigate_to_pose` | ROS2 action（`amr_interfaces/action/AmrNavigateToPose.action`） | 已有 | 不改；feedback `current_pose`（AMR 中心点）由空回调变为被消费，是进出区边沿唯一主数据源 |
| `plan` | ROS2 topic（`nav_msgs/msg/Path`，相对名） | 仅 RViz 显示 | **不订阅**（V0.20 定案，旧「新增订阅」已废弃）；`onPlan()` / `onPlanChanged()` / `D_trigger` 全部移除 |
| `robot_pose` | ROS2 topic（1 Hz） | 已有 | 不改；**禁止作主判据**（仅初始位姿 / 兜底） |
| `traffic_gates.yaml` / `MapInfo.traffic_gates_yaml` | 独立地图文件 + msg 字段（第 4 文件 / 下发 8 处链路） | — | **新增**：`resource_id` + 4 点 `polygon` + `wait_poses{l1,l2}`（无 `side` / `forward` / `trigger` / `release`）；`traffic_gate_loader` 读取 |
| `get_robot_pose` | ROS2 service | 已有 | 不改 |
| `task_command` / `task_event` | ROS2 topic | 已有 | **不改结构**（V2 分册 3.4.5 红线） |
| `traffic_request` / `traffic_response` | ROS2 topic（**已定名**，`amr_common::topic::TRAFFIC_REQUEST` / `TRAFFIC_RESPONSE`） | — | **新增**一对专用 msg |
| `TrafficRequest.msg` / `TrafficResponse.msg` | ROS2 msg（`amr_interfaces`） | — | **新增**；字段表见实现规格 §2 |
| `power_mode` / `power_level` | ROS2 topic（**已定名**，D 方案） | — | **新增** `PowerMode.msg` / `BatteryState.msg`，与路权链无耦合 |
| `AMR_TRAFFIC_REQUEST` / `AMR_TRAFFIC_RESPONSE` / `AMR_TRAFFIC_SUBSCRIBE` | MQTT 常量（替换 `mqtt_topic.h:53-54` 旧常量） | 旧常量为字符串 | **替换并启用**；补两个 `make*Topic(robot_id)` helper |
| `amr/{robot_id}/traffic_rights/request` / `response` | MQTT topic（**已定名**） | — | **新增**发布 / 订阅 |
| `amr/+/traffic_rights/request` | MQTT 通配订阅（Edge，**已定名**） | 通配能力已有 | **新增订阅**；只订上行，按 payload `op` 分发 |
| `op` = `REQUEST` / `CANCEL` / `RELEASE` | payload 必填字段（**已定案**） | — | **新增**；缺失即丢弃并记日志 |
| `request_id` / `epoch` / `lease_ttl_sec` / `eta_sec` / `queue_seq` | payload / SQL 字段 | — | **新增**；字段与 JSON 编码已定案（实现规格 §2） |
| `tbl_resource_lease` | SQLite 表 | — | **新增**（是否新增表本身仍属 TBD-020） |
| `PowerMode` / `BatteryState` | ROS2 msg（`power_mode` / `power_level`） | — | V2 新增，与路权链**无耦合**（D 方案） |

---

## 8. 实现者必读检查项

- [ ] 先读完 V2 分册 3.2（决策）与本文 §4（链路），**不要只看其中一份**。
- [ ] 修完 §5 的 4 个导航缺陷再接路权；否则「cancel → 重发」会被 supersede / epoch / 60 s 超时反复放大。
- [ ] `TrafficGate` 在 `amr_behavior_manager` 进程内（**INV-14 已定案**）；`amr_agent` 侧只补 `TrafficBridge`，**不搬业务逻辑、不新增 MQTT client、不新建独立 ROS2 包**；代码与注释中不得出现已废弃的独立模块名。
- [ ] `current_pose` 从 **action feedback** 取（填实 `amr_behavior_manager_node.cpp:125-129` 的空回调），不是新建 topic；主判据是 **AMR 中心点 `point_in_poly` 的 `inside` 进出边沿**（含滞回双阈值 + 状态过滤）；**不订阅 `plan`**，1 Hz `robot_pose` 禁止作主判据。
- [ ] 不复用 `task_command` / `task_event` 传路权；新增**已定名**的 `traffic_request` / `traffic_response`（常量定义在 `amr_common::topic`）；MQTT 用 `amr/{robot_id}/traffic_rights/request` / `response`，动作走 payload `op`。
- [ ] `NavigationClient::cancelNavigation()` 直接复用；**等 result 回调 `m_goal_active=false` 后再发新 goal**。
- [ ] 目标全程为 D；等待点是唯一例外且**位于入口端边外侧（上游）**、同侧就近（`entry_side` 边沿锁存）；`REQUESTING` 收 `GRANTED` **零动作（零 goal 重发、零停顿）**，仅 `WAITING` 收 `GRANTED` 才立即重发 D。
- [ ] 等待期间 Nav2 无活动 goal（本就**无 `plan` 订阅**）；固定 `T_retry = 10 s` 无 jitter，同 `request_id` 幂等；仅 `WAITING` 收到 `GRANTED` 立即重发 D。
- [ ] 释放四触发取最早（**出区边沿 / 任务结束 / `lease_ttl` 到期 / 显式 `CANCEL`**）；「改道即释放」**保留语义、移除独立机制**（驶离由出区边沿、停住 / 断流由 TTL 承接）；`SUCCEEDED` 只是兜底。
- [ ] Edge 侧 `PENDING` 不进部分唯一索引；显式 `cancel` 分「出队 / 释放」两态；`epoch` 丢弃迟到消息；占用状态不广播、不改 `task_state`。

---

## 9. 仍未冻结（实现者不得自行选择）

> **第七轮已从本清单剔除（已定案，不得再自行选择）**：模块落点（`amr_behavior_manager` 内 `TrafficGate`）、专用 ROS2 topic 名、MQTT topic 名、动作承载方式（payload `op`，不分层）、消息字段集合与 JSON 编码、V2 需求写入 SRS。见 **INV-14** 与 [V2 实现规格 §1](v2-implementation-spec.md)。

| 项 | 归属 | 本文件的立场 |
| --- | --- | --- |
| `queue_seq` 生成、`PENDING` 超时作废、`lease_ttl` 默认值与续租规则、表最终字段 / 是否新增 `tbl_resource_lease` | TBD-020 | 未定；实现规格给出**建议默认值**，拍板前只能用于本地调试 |
| `polygon` / `wait_poses` 字段名、`resource_id` 取值、`L_pull` / `min_release_clearance` 数值（`L_pull ≥ v_max × (RTT_拒绝_p99 + T_cancel + T_newgoal_accept) + v_max²/(2·abs(a_dec)) + d_safety`） | TBD-021 | 几何**形态**已定案（4 点凸 `polygon` 通行控制区：端边外移 / 侧边拉长 `L_pull` / 端边不压分叉点），字段与数值未定；数值由 V1.5 基准实验实测 |
| 滞回双阈值（内缩进入 / 外扩 `d_hyst` 或连续 N 帧时间窗）、边沿检测去抖；AMR 侧 `traffic_gate_loader` 读取 `traffic_gates.yaml` | TBD-038 / 039 | 未定（`crossed()` 已降级为侧边异常辅助，「改道即释放」独立机制已移除、无冷却项；039 对应 loader 读取） |
| BT 库版本、XML 路径、自定义节点注册方式 | TBD-023 | 未定；V1 无任何 BT 代码 |
| 软件急停触发源与仲裁落点 | TBD-024 | 未定 |
| `ETA` 精度与 lease 生效时刻、验收阈值 | TBD-038 | 未定 |

另：**SRS 已纳入 V2 需求**（第七轮授权写入 V0.19，2026-11-22 起为 V0.20）；**ADR-0002 仍未产出**、**V1.5 尚未自验**（项目所有者说明稍后进行）。这两项是实施前的流程前置，不是本文能代劳的。

---

## 10. 与其他文档的关系 / 引用约定

| 文档 | 角色 |
| --- | --- |
| `v2-resource-arbitration-and-bt.md` 3.2 | **决策权威**：按需触发主模型（4 点通行控制区 + 中心点进出边沿 + 无 `plan` 订阅）、四释放触发、Edge FIFO/TTL、独立 `traffic_gates.yaml` 地图几何、验收清单 |
| `../version-roadmap.md` INV-13 / TBD 表 | **不变式与待办权威** |
| `../../assets/v2-traffic-dataflow.svg` | 本文 §3 / §4 的**人类阅读版**图表示 |
| `../../assets/v2-traffic-sequence.svg` | 本文 §6 的**人类阅读版**图表示 |
| [`v2-traffic-diagrams.md`](v2-traffic-diagrams.md) | 两张 SVG 的 **mermaid 纯文字转写**，供写代码的 agent / 纯文本工具读取；只提供结构视图，语义仍以本文 §4 / §6 为准 |
| `v2-implementation-spec.md` | **实现级权威**：命名 / 消息字段 / 状态机 / 伪代码 / 全场景流程 / 配置项 / 自检清单 |
| `../../requirements/srs.md` V0.20 第 4.10 / 4.11 节 | V2 功能需求（已正式纳入需求基线） |
| `../../requirements/interface-contract.md` 第 8 节 | V2 路权 MQTT / ROS2 接口契约（已同步） |
| `../../development/project-review.md` | 已记录 `TrafficBridge` 空壳与注释订阅的事实 |
| `../amr.md` 3.4 | V2 路权逻辑内嵌于 `amr_behavior_manager` 的职责定义（已改写，不再有独立 traffic 模块） |

**引用约定**：本文编号一律写「**控制链 §N〈标题〉**」；V2 分册写「**V2 分册 3.2.2**」这类完整路径；方案稿写「**方案稿 §N〈标题〉**」、判定稿写「**判定稿 §N〈标题〉**」。聊天与文档中**不使用裸编号**（两份设计稿都有 §7，含义不同）。

> **本文定位**：把「已冻结的决策」翻译成「可连线的接口与可执行的调用顺序」；**实现级细节（字段 / 状态机 / 伪代码）在 `v2-implementation-spec.md`**。若本文与 V2 分册冲突，**以 V2 分册为准并回报冲突**。

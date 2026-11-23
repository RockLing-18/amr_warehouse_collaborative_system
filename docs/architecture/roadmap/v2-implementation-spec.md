# V2 实现规格与功能流程（实现者开工依据）

> ⚠️ **2026-11-22 V0.20 定案变更**：本文已按项目所有者对 V2 窄道路权的 **8 项拍板**同步了**几何 / 主判据 / 订阅**口径 —— 主判据改为「AMR 中心点进出 4 点通行控制区的 `point_in_poly(center, polygon)` `inside` 边沿」，几何改为「侧边沿通道拉长 `L_pull` + 端边外移 `min_release_clearance` 的 4 点凸 `polygon` 通行控制区」，并**移除 `plan` 订阅与 `onPlan()` / `onPlanChanged()` 预判**。旧「有向触发段 / 外移释放线 / `crossed()` 主判据 / `onPlan()` 提前预判 / 越释放线即释放」口径**已废弃**，仅作历史对比时保留并标注，实现者不得照旧口径编码。唯一权威口径见 `.workbuddy/artifacts/traffic-gate-8-decisions-frozen.md`（第十三轮 8 项定案记录）。

> **状态：已定案部分的实施规格**，2026-11-20 第七轮新增。
> **权威链**：`srs.md` V0.20（需求 / 决策）→ `version-roadmap.md` INV-13 / **INV-14**（不变式 / 命名）→ `v2-resource-arbitration-and-bt.md` 3.2（设计取舍）→ `v2-traffic-control-chain.md`（谁连谁）→ **本文（字段 / 状态机 / 伪代码 / 全场景流程）**。冲突时以上游为准并回报。
> **本文件不新增设计决策**：凡标 `[TBD-xxx]` 的都是**未冻结**，给出的数值只是本地调试建议默认值，实现者不得当作定案；凡标「已冻结」的不得自行更改。
> **读法**：§0 定案速查 → §1 命名 → §2 消息 → §3 文件清单 → §4 状态机 → §5/§6 伪代码 → §7 全场景流程 → §8 配置 → §9 异常 → §10 前置阻塞 → §11 自检。

## 章节速查

| 章节 | 讲什么 | 实现者拿来干什么 |
| --- | --- | --- |
| §0 | 本轮定案 vs 仍待冻结 | 一眼看清哪些不许猜 |
| §1 | ROS2 / MQTT / SQL 命名总表 | 建常量、建 topic |
| §2 | 4 个 `.msg` 与 JSON 编码 | 直接复制建消息文件 |
| §3 | 模块 / 类 / 待改文件清单 | 排任务、改 CMake |
| §4 | `TrafficGate` 单资源状态机 | 写类骨架 |
| §5 | AMR 侧逐步伪代码 | 写 `traffic_gate.cpp` |
| §6 | Edge 侧仲裁伪代码 | 写 `traffic_rights_service` / `ResourceBroker` |
| §7 | 7 个全场景功能流程 | 对照联调与验收 |
| §8 | 配置项与建议默认值 | 建 yaml |
| §9 | 错误 / 竞态 / 边界处理表 | 补异常分支 |
| §10 | 4 个导航缺陷修复要点 | 前置阻塞项 |
| §11 | 实现顺序与自检清单 | 开工 / 提测 |
| §12 | 仍待冻结清单 | 边界 |

---

## 0. 定案速查（第七轮 + 2026-11-22 V0.20 8 项拍板）

| # | 事项 | 状态 | 落地位置 |
| --- | --- | --- | --- |
| 1 | 路权逻辑 = `amr_behavior_manager` 包内 `TrafficGate`，**不设独立模块 / 包** | **已冻结** | INV-14 / SRS D-057 |
| 2 | 全仓清理已废弃的独立交通模块命名 | **已执行** | 本轮已改 11 份文档 |
| 3 | ROS2 topic `traffic_request` / `traffic_response` | **已冻结** | INV-14 / SRS D-058 / 本文 §1.1 |
| 4 | MQTT `amr/{robot_id}/traffic_rights/request` / `response`；Edge 订阅 `amr/+/traffic_rights/request` | **已冻结** | INV-14 / SRS D-059 / 本文 §1.2 |
| 5 | 动作由 payload `op`（`REQUEST`/`CANCEL`/`RELEASE`）承载，**不采用动作分层 topic** | **已冻结** | V2 分册 3.2.5 / 本文 §2 |
| 6 | 消息字段集合与 JSON 编码 | **已冻结** | `interface-contract.md` §8 / 本文 §2 |
| 7 | V2 需求正式写入 SRS（V0.19 首次纳入，现 V0.20） | **已执行** | `srs.md` 1.6 / 4.10 / 4.11 / 6.7 / 7 章 |
| 8 | 运行时按需触发、目标全程 D、中心点**进区申请 / 出区释放**（`inside` 边沿）、固定 10 s、Edge FIFO+TTL | **已冻结**（第五轮，V0.20 改判据） | INV-13 / V2 分册 3.2 |
| 9 | 电量 B+D、充电 B、BT C | **已冻结** | TBD-035 / 036；V2 分册 3.3 / 3.4 |
| 10 | 数值参数、表最终字段、区域 / 数据字段名（`polygon` / `wait_poses` / `L_pull`）、BT 选型、急停落点 | **未冻结** | §12，见 TBD-020/021/022/023/024/038/039 |

> **一句话边界**：接口、命名、流程、状态机已经可以直接开写；**只有数值和少量技术选型还要等拍板**。实现时把数值集中到 §8 配置表，拍板后改配置即可，不要散落在代码里。V0.20 的 8 项几何 / 判据 / 订阅定案已同步进本文 §1~§12；与第七轮口径冲突时，以顶部 V0.20 变更指针及第十三轮定案记录为准。

---

## 1. 命名与接口总表（已冻结）

### 1.1 AMR 内部 ROS2

常量统一定义在 `robot_ws/src/amr_common/include/amr_common/ros_interface_names.h`，**一律相对名**，由 launch namespace 解析为 `/{robot_id}/...`（`interface-contract.md` 第 5 节）。

| 常量（`amr_common::topic::`） | 名称 | 消息类型 | 方向 | QoS |
| --- | --- | --- | --- | --- |
| `TRAFFIC_REQUEST` | `traffic_request` | `amr_interfaces/msg/TrafficRequest` | `amr_behavior_manager` → `amr_agent` | reliable / volatile |
| `TRAFFIC_RESPONSE` | `traffic_response` | `amr_interfaces/msg/TrafficResponse` | `amr_agent` → `amr_behavior_manager` | reliable / volatile |
| `POWER_MODE` | `power_mode` | `amr_interfaces/msg/PowerMode` | `amr_behavior_manager` → `amr_power_manager` | reliable / volatile |
| `POWER_LEVEL` | `power_level` | `amr_interfaces/msg/BatteryState` | `amr_power_manager` → `amr_behavior_manager` | reliable / volatile |

**禁止**：复用 `task_command` / `task_event`；新增第二个 MQTT client；新增独立 `ComputePath` / `FollowPath` Action Client 或 `NavigateThroughPoses`；**订阅 Nav2 `plan`**（V0.20 定案移除，原「只读订阅」口径已废弃）。现有 `amr_navigate_to_pose` action、`robot_pose`（1 Hz，仅初始位姿/兜底）、`get_robot_pose` service 全部**不改**；`plan` 不再订阅，`onPlan()` / `onPlanChanged()` 不再存在，提前量改由拉长区域几何自带（§5.2）。

### 1.2 MQTT

常量统一定义在 `common/mqtt/mqtt_topic.h`，QoS 1。

| 常量（`mqtt_topic::`） | 值 | 方向 | 说明 |
| --- | --- | --- | --- |
| `AMR_TRAFFIC_REQUEST` | `"amr/{}/traffic_rights/request"` | AMR → Edge | 三种 `op` 共用同一条 topic |
| `AMR_TRAFFIC_RESPONSE` | `"amr/{}/traffic_rights/response"` | Edge → AMR | 单播，不广播 |
| `AMR_TRAFFIC_SUBSCRIBE` | `"amr/+/traffic_rights/request"` | Edge 订阅 | `robot_id` 一层通配，新增机器人零配置 |

helper：`makeAmrTrafficRequestTopic(robot_id)` / `makeAmrTrafficResponseTopic(robot_id)`。

**V1 常量替换**：删 `TRAFFIC_RIGHTS_REQ = "amr/traffic_rights/request"` 与 `TRAFFIC_RIGHTS_RSP_PREFIX = "amr/traffic_rights/response/"`。原因：两者都缺 `robot_id` 层（3 级），与 `amr/+/traffic_rights/request`（4 级）不匹配；且与 V1 其它 `amr/{robot_id}/...` 主题风格不一致。

### 1.3 SQLite

`tbl_resource_lease`（**是否新建表、最终字段仍属 TBD-020**）。建议 DDL 见 `interface-contract.md` §8.6。硬约束：`PENDING` 不进部分唯一索引；`GRANTED` / `HELD` 上 `(resource_type, resource_id)` 唯一；Edge 重启可重建。

### 1.4 三条红线（做错即返工）

1. `amr_agent` 不碰位姿、不判几何、不做业务决策 —— 它只是 `TrafficBridge` 透明桥。
2. `amr_behavior_manager` 不直连 MQTT —— 否则出现第二套心跳 / Last Will / 待补报。
3. Edge 不知道 Nav2 存在 —— 只认 `resource_id` / `request_id` / `epoch` / `lease_ttl_sec`，不下发坐标与途经点。

---

## 2. 消息结构（直接复制建文件）

> 完整字段表与 JSON 示例见 `interface-contract.md` §8。本节给 `.msg` 原文，便于实现者直接落盘。

### 2.1 `amr_interfaces/msg/TrafficRequest.msg`

```text
builtin_interfaces/Time stamp
string   op              # REQUEST | CANCEL | RELEASE（必填，缺失即丢弃并记日志）
string   request_id      # 幂等键，建议 {robot_id}-{resource_id}-{seq}
string   robot_id
string   resource_id     # 区域 id，不是坐标
int64    epoch           # 请求代次；Edge 丢弃陈旧消息
float64  eta_sec         # 预计进入时刻（相对仿真时间，秒），CANCEL/RELEASE 可填 0
float64  lease_ttl_sec   # 权限占用时限
```

### 2.2 `amr_interfaces/msg/TrafficResponse.msg`

```text
builtin_interfaces/Time stamp
string   request_id
string   robot_id
string   resource_id
int64    epoch
string   result          # GRANTED | REJECTED | PENDING | CANCELLED | RELEASED | EXPIRED
int32    queue_seq       # PENDING 时的 FIFO 序号；无排队填 -1
float64  lease_expires_at
string   reason
```

> `queue_seq` 在本 `.msg` 原文中已存在（V0.20 定案 4）：多车等待点分散的备选方案用它做 `queue_seq % N_side` 组内分配，**不新增任何 `.msg` 字段**，字段集合仍以 `interface-contract.md` §8 为准。

### 2.3 `amr_interfaces/msg/PowerMode.msg` / `BatteryState.msg`（D 方案）

```text
# PowerMode.msg
int32 MODE_IDLE=0
int32 MODE_WORK=1
int32 MODE_CHARGING=2
builtin_interfaces/Time stamp
int32 mode

# BatteryState.msg
builtin_interfaces/Time stamp
float32 battery      # 0..100
bool    is_low
```

### 2.4 JSON 编码要点

- 字段一律 `snake_case`（SRS D-008）；`.msg` 的 `stamp` 在 JSON 写为 `timestamp`（仿真时间秒，浮点）。
- `op` 三态共用一条上行 topic；**下行 response 不带 `op`**（AMR 只订阅一条 `.../response`，无需分发）。
- `request_id` 在同一趟任务的同一 `resource_id` 上**复用**，`CANCEL` / `RELEASE` / 10 s 幂等重发都带同一个值。
- `epoch`：每次「cancel 当前导航 / 重发 D」递增，用于丢弃迟到的旧响应。

### 2.5 `amr_agent` 的 `TrafficBridge` 转换规则

| 方向 | 输入 | 输出 | 规则 |
| --- | --- | --- | --- |
| 出站 | ROS2 `traffic_request` | MQTT `amr/{robot_id}/traffic_rights/request` | 原样序列化，不增删字段、不做判定 |
| 入站 | MQTT `amr/{robot_id}/traffic_rights/response` | ROS2 `traffic_response` | 校验 topic 中 `robot_id` == payload `robot_id`，不一致丢弃并记日志 |

范式照抄 `amr_agent/src/bridge/task_bridge.cpp:33-91`：`registerExternalHandler()` + `setExternalSubscribe()`；V1 已有 `class TrafficBridge;` 前向声明（`amr_agent_node.h:18,38`），只需补定义与实例化，不发明新机制。

---

## 3. 模块 / 类 / 待改文件清单

### 3.1 AMR 侧（进程归属：见 INV-14）

| 文件 | 动作 | 内容 |
| --- | --- | --- |
| `amr_common/include/amr_common/ros_interface_names.h` | 改 | 加 §1.1 的 4 个常量 |
| `amr_interfaces/msg/{TrafficRequest,TrafficResponse,PowerMode,BatteryState}.msg` | 新增 | §2 原文 |
| `amr_interfaces/CMakeLists.txt` | 改 | `rosidl_generate_interfaces` 加 4 个 msg |
| `amr_agent/src/bridge/traffic_bridge.{h,cpp}` | 新增（补空壳） | §2.5 透明桥 |
| `amr_agent/src/amr_agent_node.{h,cpp}` | 改 | 实例化 `TrafficBridge`；删前向声明 |
| `amr_agent/src/communication/edge_communication.cpp:152-157`、`.h:48-51` | 改 | 启用被注释的 response handler 注册，订阅 `makeAmrTrafficResponseTopic(robot_id)` |
| `amr_behavior_manager/src/traffic_gate.{h,cpp}` | **新增（核心）** | §4 状态机 + §5 全部伪代码 |
| `amr_behavior_manager/src/amr_behavior_manager_node.cpp:125-129` | 改 | 填实 feedback 空回调，把 `current_pose` 推给 `TrafficGate` |
| `amr_behavior_manager/src/traffic_gate_loader.{h,cpp}` | **新增（核心）** | 新 `traffic_gate_loader` 读独立 `traffic_gates.yaml` 的 4 点 `polygon` + `wait_poses{l1,l2}`，派生 `long_axis` / `centroid`（TBD-039 对应 loader 读取，见 §3.3）；旧「`zone_station_loader` 读有向触发段 / 释放线」口径已废弃 |
| `amr_behavior_manager/src/bt_nodes/{request_on_approach,wait_and_retry,release_traffic_right}.cpp` | 新增 | BT 节点（技术选型 TBD-023） |
| `amr_behavior_manager/src/navigation_client.cpp` 等 | 改 | 修 §10 的 4 个缺陷（前置阻塞） |
| `amr_power_manager/`（新 ROS2 包） | 新增 | 单订阅 `power_mode` + 积分定时器 + 单发布 `power_level` |
| `amr_behavior_manager/src/power_mode_publisher.cpp` | 新增 | 按任务状态发 IDLE/WORK/CHARGING |
| `amr_behavior_manager/CMakeLists.txt` / `package.xml` | 改 | 加新源文件与 `amr_interfaces` 依赖；**不再需要 `nav_msgs::msg::Path` 依赖**（V0.20 移除 `plan` 订阅） |

**`TrafficGate` 类骨架（建议，非冻结命名）**：

```cpp
class TrafficGate
{
public:
    // 由 amr_behavior_manager_node 在进程内注入，全部是已有对象
    // V0.20：不再注入 plan 订阅（plan_sub 已废弃），提前量由 4 点控制区几何自带
    TrafficGate(NavigationClient* nav, TrafficGateLoader* loader,
                rclcpp::Publisher<TrafficRequest>::SharedPtr req_pub,
                rclcpp::Subscription<TrafficResponse>::SharedPtr rsp_sub);

    void onFeedback(const geometry_msgs::msg::PoseStamped& pose);  // 填实 V1 空回调；主判据 = 中心点 inside 边沿
    void onResponse(const TrafficResponse& rsp);
    void onTaskFinished(NavResult result);                         // SUCCEEDED / ABORTED 兜底
    void onTaskCancelled();                                        // 显式 CANCEL

private:
    struct GateContext {
        std::string resource_id;
        std::string request_id;
        int64_t epoch = 0;
        GateState state = GateState::IDLE;
        Polygon polygon;                 // 4 点凸通行控制区，点序冻结：P1→P2 端边 L1 / P2→P3 长轴 / P3→P4 端边 L2
        Vec2 long_axis;                  // normalize(P3-P2)，点序派生、不落盘
        Vec2 centroid;                   // mean(P1..P4)
        std::map<std::string, std::vector<Pose>> wait_poses;  // {l1:[...], l2:[...]}，每侧 >= 1
        std::string entry_side;          // 本次穿越周期锁存，绝不每帧重算（否则中部 s≈0 会翻侧）
        Pose wait_pose;                  // = wait_poses[entry_side] 组内 argmin 结果（入口端边外侧上游、锁存）
        bool inside_prev = false;        // 中心点 inside 上一帧，用于边沿检测（配合滞回双阈值）
        bool already_handled = false;    // already_passed_this_task
        rclcpp::Time last_request_at;
    };
    std::unordered_map<std::string, GateContext> gates_;  // key = resource_id
    // ... §5 各函数（onPlan / onPlanChanged 已移除）
};
```

### 3.2 Edge 侧

| 文件 | 动作 | 内容 |
| --- | --- | --- |
| `common/mqtt/mqtt_topic.h` | 改 | §1.2 常量 + 2 个 helper |
| `edge_server/src/communication/amr_communication.cpp:110-115` | 改 | `setSubscribe(AMR_TRAFFIC_SUBSCRIBE, ...)`，按 `op` 分发 |
| `edge_server/src/service/traffic_rights_service.{h,cpp}` | 新增 | §6.1 消息分发 + 幂等 + 单播响应 |
| `edge_server/src/service/resource_broker.{h,cpp}` | 新增 | §6.2~6.5 FIFO / lease / TTL / 重启恢复 |
| `edge_server/src/database/sqlite_db.cpp` | 改 | 建表 DDL + CRUD（TBD-020 表字段待定） |
| `edge_server/config/edge_server.yaml` | 改 | §8 配置项 |
| `edge_server/src/service/idle_point_allocator.cpp` | 改 | 充电点占用升级为 lease（3.1.5） |

### 3.3 地图与工具链（V0.20 定案 8：独立 `traffic_gates.yaml` + `MapInfo` 加字段 + 8 处下发链路）

**数据结构**：独立 `data/warehouse_01/traffic_gates.yaml`，字段为 `resource_id` + 4 点 `polygon` + `wait_poses{l1:[...], l2:[...]}`（每侧 ≥1）；**无 `side` / `forward` / `trigger` / `release` 字段**，正反一份数据，进入侧角色由运行时 `dot(enter_pose - centroid, long_axis)` 符号派生（§5.3）。点序冻结：`P1→P2` 端边 L1 / `P2→P3` 长轴 / `P3→P4` 端边 L2。草案：

```yaml
traffic_gates:
  - resource_id: crossing_01
    polygon:                        # 4 点凸，点序冻结
      - [x1, y1]                    # P1
      - [x2, y2]                    # P2
      - [x3, y3]                    # P3
      - [x4, y4]                    # P4
    wait_poses:
      l1:                           # L1 端边外侧（上游），>= 1 个
        - {x: .., y: .., yaw: ..}   # yaw 由长轴派生，不人工填
      l2:                           # L2 端边外侧（上游），>= 1 个
        - {x: .., y: .., yaw: ..}
```

**打包下发 8 处链路**（显式字段保证地图包版本一致性）：

| 文件 | 动作 | 内容 |
| --- | --- | --- |
| `data/warehouse_01/traffic_gates.yaml` | **新增** | 上述独立文件；`zones.yaml` / `stations.yaml` / `metadata.yaml` **不改**（旧 `NARROW` zone / `TRAFFIC_WAIT` 等待点 / `side` 口径已废弃）；不新增路线 / 顺序表；地图包版本升级（TBD-003） |
| `REQUIRED_WAREHOUSE_FILES` / `metadata.py` / `package.py` | 改 | 将 `traffic_gates.yaml` 纳入必需文件、元数据与打包清单 |
| `tool/map_tool/validator.py` | 改 | 现状 `:59-72` **只检查文件存在**；新增本节末尾「7 条内容校验」 |
| `amr_interfaces/msg/MapInfo.msg` | 改 | 加 `traffic_gates_yaml` 字段，随地图包下发 |
| `map_manager.cpp::publishMapInfo()` | 改 | 填充 `traffic_gates_yaml` |
| `amr_behavior_manager` 的 MapInfo 回调 | 改 | 接收 `traffic_gates_yaml` 并转交 loader |
| `amr_behavior_manager/src/traffic_gate_loader.{h,cpp}` | **新增** | 解析 4 点 `polygon` + `wait_poses{l1,l2}`，派生 `long_axis` / `centroid`（TBD-039 对应 loader 读取，见 §3.1） |

**工具侧 `traffic_gates_editor`（两阶段交互）**：阶段 A 录 4 点 zone（`start zone` → rviz 点 4 下 → `finish zone` 校验 `size()==4` / 凸 / 点序连续 / 面积 > 0 → 终端填 `resource_id`），阶段 B 录等待点（`start waits` → rviz 点 N 下 → `finish waits` 按几何自动归档到 L1 / L2 外侧两组、每侧 ≥1、`yaw` 由长轴派生 → `save` 写 `traffic_gates.yaml`）。**复用** `ClickedPointReceiver`（`/clicked_point`，`PointStamped` 只有 x/y）与 `ZoneMarkerPublisher` 的 polygon 渲染；**不复用** `zone_editor` 节点本体 / `YamlZoneManager` / `ZoneInfo` / `selectZoneType()`（带 `goods_ids` / `type`，会污染 V1 类型菜单与 `zones.yaml`）。新建 `TrafficGate{resource_id, polygon, wait_poses}`；**不新建 station、不复用 `WAIT`、不需要 `TRAFFIC_WAIT` 类型**（`goods_point_resolver.cpp:77-97` 会串改）。

**`validator.py` 新增 7 条内容校验**（现状 `:59-72` 只检查文件存在，以下全部为新增）：① `polygon.size()==4`、凸四边形、点序连续、面积 > 0；② 两条端边近似平行、与长轴近似垂直，间距 `L_pull >= 配置的拉长下限`；③ 端边与 `walls.yaml` 线段不冲突、所在通道宽度 ≥ 车宽 + 余量；④ `wait_poses.l1` / `.l2` 各 ≥1 且位于对应端边外侧（上游）、不在 `polygon` 内，**同侧相邻等待点间距 ≥ 车宽 + `d_queue_spacing`**（多车不叠车，定案 4）；⑤ 端边到真实窄道出口净距 ≥ `min_release_clearance`；⑥ 区域不与分叉点之前的公共通道重叠（保 S6 零误触发）；⑦ `resource_id` 全局唯一、非空。

---

## 4. `TrafficGate` 单资源状态机

**每个窄道 `resource_id` 一个独立 `GateContext`**，谁先遇到谁先申请，不预排序列（INV-13）。一趟任务开始时清空 `gates_`，结束时全部释放。

| 状态 | 含义 | Nav2 活动 goal | `cmd_vel` |
| --- | --- | --- | --- |
| `IDLE` | 未遇到该 gate | 原 `NavigateToPose(D)` | 正常 |
| `REQUESTING` | 已发 `op=REQUEST`，等响应 | **原 goal 保持不变、车继续行驶** | 正常 |
| `HELD` | 已授权，正在通过窄道 | 原 `NavigateToPose(D)` | 正常 |
| `CANCELING` | 被拒，`cancelNavigation()` 已发，等 result 回调 | 取消中 | 归零 |
| `WAITING` | 已到等待点停稳 | **无活动 goal（Nav2 无规划输出）** | 归零（模型 A） |
| `RESUMING` | 收到 `GRANTED`，已重发 `NavigateToPose(D)` | 原 goal D | 正常 |
| `RELEASED` | 四触发之一已释放 | 原 goal（不因释放而改） | 正常 |

```text
                    ┌────────────── GRANTED（正常，零动作）──────────────┐
                    │                                                    ▼
IDLE ──进区──> REQUESTING ───────────────────────────────────────────> HELD
                    │                                                    │
                    └─ REJECTED / 超时 ─> CANCELING ─(result CANCELED)─> WAITING
                                                                          │
                                              ┌── GRANTED（事件驱动）─────┘
                                              ▼
                                          RESUMING ──> HELD
                                              │
                    四触发任一 ────────────────┴──────────────> RELEASED
```

> 图例（V0.20）：「进区」= 中心点 `point_in_poly` 的 `inside` `false→true` 边沿（含滞回）；「四触发」= 出区边沿 / 任务结束 / 显式 `CANCEL` / `lease_ttl` 到期，取最早者。改道不设独立触发，由出区边沿或 TTL 承接（§5.9）。

**事件 → 动作表**：

| 事件 | 当前状态 | 动作 | 新状态 |
| --- | --- | --- | --- |
| 中心点进区边沿（`inside` `false→true`，含滞回） | `IDLE` | 发 `op=REQUEST`（`eta_sec` + `lease_ttl_sec`），**不动 goal**；锁存 `entry_side`、选 `wait_pose` | `REQUESTING` |
| `response.result = GRANTED` | `REQUESTING` | **零动作**（记日志 `early_granted`） | `HELD` |
| `response.result = REJECTED` / `PENDING` 超时 | `REQUESTING` | `cancelNavigation()`，等 result 回调 | `CANCELING` |
| result 回调 `m_goal_active = false` | `CANCELING` | `navigateToPose(wait_pose)`（进入侧 `wait_poses[entry_side]` 组内 `argmin`，入口端边外侧上游） | `CANCELING` → 到点后 `WAITING` |
| 每 `T_retry = 10 s` | `WAITING` | 同 `request_id` 幂等重发 `REQUEST` | `WAITING` |
| `response.result = GRANTED` | `WAITING` | **立即** `navigateToPose(D)`，不等轮询 | `RESUMING` → `HELD` |
| 出区边沿（`inside` `true→false`）/ 任务结束 / 显式 `CANCEL` / TTL 四触发取最早；改道后驶离区域归此项 | `HELD` / `RESUMING` | 发 `op=RELEASE`（+ 必要时 `CANCEL`） | `RELEASED` |
| 任务取消 / 主动放弃 | 任意 | 显式 `op=CANCEL`（排队中出队 / 已授权释放）；改道不由此触发，见 §5.9 | `RELEASED` |

---

## 5. AMR 侧逐步伪代码

### 5.1 `pointInPoly()` + `inside` 边沿 + 滞回 —— 进出区域主判据（V0.20 定案 1）

```cpp
// 主判据：AMR 中心点是否在 4 点凸通行控制区内（不是跨线事件、不是 footprint）
bool pointInPoly(const Vec2& center, const Polygon& polygon);   // 射线法 / 叉积，凸 4 点

// 边沿检测 + 滞回双阈值：进入用内缩 d_hyst、离开用外扩 d_hyst（阈值 [TBD-038]）
// 两条阈值错开，避免边界位姿噪声让 inside 反复翻转
enum class ZoneEdge { NONE, ENTER, EXIT };

ZoneEdge TrafficGate::detectEdge(const GateContext& g, const Vec2& center)
{
    bool inside;
    if (g.inside_prev) {
        inside = pointInPoly(center, expand(g.polygon, d_hyst));   // 已在区内：外扩，完全出去才置 false
    } else {
        inside = pointInPoly(center, shrink(g.polygon, d_hyst));   // 在区外：内缩，确实进来才置 true
    }
    if (inside == g.inside_prev) return ZoneEdge::NONE;             // 只在翻转时发事件，逐帧不重复 REQUEST / RELEASE
    return inside ? ZoneEdge::ENTER : ZoneEdge::EXIT;               // false→true 申请 / true→false 释放
}
```

> **主判据口径**：AMR **中心点** `point_in_poly(center, polygon)` 的 `inside` **状态边沿** —— 进区 `false→true` 发 `op=REQUEST`、出区 `true→false` 发 `op=RELEASE`。**不是跨线事件、不是 footprint**。数据源仍是 `amr_navigate_to_pose` action feedback 的 `current_pose`（`amr_behavior_manager_node.cpp:125-129` 空回调填实），**1 Hz `robot_pose` 仍禁止作主判据**。
>
> **抗抖不可省**：中心点只是比 footprint 稳，抖动来自「逐帧读状态」本身（车停边界附近 / 被拒后去等待点会再次穿越边界）。故仍需**边沿检测 + 滞回双阈值**（内缩进入 / 外扩离开，或等价「连续 N 帧一致」时间窗去抖），阈值属 TBD-038，只是取值可比 footprint 小。
>
> **方向无关是实打实收益**：掉头从入口退出天然触发「出区」，原「`onFeedback()` 无反向退出分支、需补 `crossed()` 双符号语义」的缺口自动消失（历史口径，已废弃）。
>
> **`crossed()` 已废弃作主判据**：降级为辅助，仅用于识别「从侧边穿入」异常（配合 §5.3 的 `s` 符号兜底 + `LOG_WARN`）与 validator 可视化，不再参与进 / 出区判定。旧「入口 `trigger` / 出口 `release` 方向取反」的有向跨线口径一并废弃。
>
> **几何静态保证**：端边外移 `min_release_clearance` ⇒ 「中心出区 ⇒ 车尾已出真实窄道」，无需逐帧 footprint 求交。

### 5.2 无 `plan` 订阅 · `L_pull` 自带提前量（V0.20 定案 6，`onPlan()` 已整节移除）

> **本节不再有伪代码**。V0.20 拍板把 Nav2 `plan` 订阅、`plan_sub` 构造参数、`onPlan()`、`pathIntersectsWithin`、`arcLengthToGate` 以及 `D_trigger` 相关配置项与文字**全部移除**（已废弃）。S1 场景不再有「`plan` 提前预判」这一跳，`robot_pose` / `plan` 都不参与申请时机判定。

**提前量改由几何自带**：4 点通行控制区的侧边沿通道拉长 `L_pull`、端边外移 `min_release_clearance`，申请在「中心点进区边沿」发出，车从进区到真实窄道口仍有 `L_pull` 的行驶时间，足以容纳 MQTT `RTT_拒绝_p99` + `cancel` + 新 goal 接受。这是定案 6 带来的真实简化：少订阅一个 topic、少一套路径相交逻辑。

`L_pull` 下限（数值留 TBD-021 / 038，须 V1.5 基准实验实测）：

- `L_pull >= v_max × (RTT_拒绝_p99 + T_cancel + T_newgoal_accept) + v_max² / (2 × abs(a_dec)) + d_safety`；
- 代入 `nav2_params.yaml:146` `max_vel_x=0.26`、`:157` `decel_lim_x=-0.5`，约 0.8~1.0 m 量级，**含「拒绝 → cancel → 新 goal」一跳**；
- 端边**不得压到分叉点之前**：Nav2 走绕行路的车根本不进区 ⇒ S6「零申请、零停顿」天然成立。

> `D_trigger`（提前申请距离）概念**已废弃**，转译为区域拉长量 `L_pull`（§8 配置表）。历史对比：第七轮口径由 `onPlan()` 在 `D_trigger` 内预判路径相交，V0.20 起由几何承担，不再订阅 `plan`。

### 5.3 `onFeedback()` —— 中心点进出区域边沿 + 状态过滤 + 进入侧计算（V0.20 定案 1 / 3 / 5）

```cpp
void TrafficGate::onFeedback(const PoseStamped& pose)
{
    const Vec2 center = pose.pose.xy;                 // 中心点（AMR 中心，不是 footprint）
    for (auto& [rid, g] : gates_) {
        ZoneEdge e = detectEdge(g, center);           // §5.1：inside 翻转 + 滞回双阈值
        if (e == ZoneEdge::NONE) continue;            // 逐帧不重复事件

        if (e == ZoneEdge::ENTER) {                  // ① 进区 false→true：申请
            // 状态过滤：HELD / RESUMING 已持权、REQUESTING / WAITING / CANCELING / RELEASED 已在处理 → 忽略「再进入」
            if (g.state == GateState::IDLE && !g.already_handled) {
                g.entry_side = calcEntrySide(g, center);   // 边沿时刻算一次并锁存（§5.3.1）
                g.wait_pose  = pickWaitPose(g, center);    // 仅同侧组内 argmin（§5.3.1）
                emitRequest(g, "REQUEST");            // 不动 goal，车继续沿原 NavigateToPose(D)
                g.state = GateState::REQUESTING;
                log("enter_request", rid, g.entry_side);
            }
        } else {                                         // ② 出区 true→false：释放
            // 状态过滤：未持权（IDLE / REQUESTING / CANCELING / WAITING / RELEASED）忽略「离开」
            if (g.state == GateState::HELD || g.state == GateState::RESUMING) {
                emitRequest(g, "RELEASE");
                g.state = GateState::RELEASED;
                g.already_handled = true;
                log("release_by_exit_edge", rid);
            }
        }
        g.inside_prev = !g.inside_prev;               // 翻转锁存（与滞回阈值配合）
    }
}

// 进入侧算法（定案 3）：不用 crossed() 作主判据，由点序派生的长轴 / 质心算符号
std::string TrafficGate::calcEntrySide(const GateContext& g, const Vec2& enter_pose)
{
    double s = dot(enter_pose - g.centroid, g.long_axis);   // long_axis = normalize(P3-P2)
    return (s < 0.0) ? "l1" : "l2";                          // s<0 → L1 / wait_poses.l1；s>0 → L2 / .l2
}

Pose TrafficGate::pickWaitPose(const GateContext& g, const Vec2& enter_pose)
{
    // 侧边异常穿入：validator 已约束侧边不与公共通道重叠，属异常；
    // 辅助 crossed(prev,cur,edge) 命中侧边时按 s 符号兜底 + LOG_WARN（其主判据口径已废弃，见 §5.1），不拒绝服务
    const auto& cands = g.wait_poses[g.entry_side];           // 只在锁存的进入侧一组里取
    return *std::min_element(cands.begin(), cands.end(),
        [&](const Pose& a, const Pose& b) { return dist(enter_pose, a) < dist(enter_pose, b); });
}
```

> **三条必须写死的约定（定案 3）**：① `entry_side` 在本次穿越周期内**锁存**（与 `already_handled` 同生命周期），**绝不每帧重算** —— 否则车走到区域中部 `s≈0` 时会翻侧、等待点选到对面；② 侧边异常穿入按 `s` 符号兜底 + `LOG_WARN`（辅助 `crossed()` 仅此用途，其**作为主判据的口径已废弃**）；③ 等待点必须在**入口端边外侧（上游）**，等待路径不得穿过真实窄道，由 editor 阶段 B 几何归档 + `validator.py` 静态校验保证（§3.3）。
>
> **多车等待点分配（定案 4）**：每侧 `wait_poses{l1:[...], l2:[...]}` ≥1 个点，默认组内 `argmin dist` 就近（纯 `TrafficGate` 本地计算，Edge 不参与、不加字段）；备选仅实测叠车时启用 `queue_seq % N_side` 分散（`queue_seq` 已在 §2.2 `TrafficResponse.msg` 原文，**不新增 `.msg` 字段**）。`validator.py` 静态保证同侧相邻等待点间距 ≥ 车宽 + `d_queue_spacing`（§3.3）。**不新建 station、不复用 `WAIT`、不需要 `TRAFFIC_WAIT` 类型**。
>
> **S1 / S2 必须区分（定案 5）**：`REQUESTING` 收 `GRANTED` **零动作**（不减速 / 不 cancel / 不重发 goal，AC-020 零 goal 重发零停顿）；只有 `WAITING` 获批才**事件驱动立即** `navigateToPose(D)` → `RESUMING → HELD`。`cancelNavigation()` 后必须等 result 回调把 `m_goal_active=false` 再发等待点（§5.6）。
>
> **数据源**：`current_pose` 来自 `amr_navigate_to_pose` **action feedback**（`AmrNavigateToPose.action:21` → `navigation_manager.cpp:87-116` → `navigation_server.cpp:419-438` → `navigation_client.cpp:62-65`），**不是独立 ROS2 topic**。`amr_behavior_manager_node.cpp:125-129` 的 `(void)feedback;` 必须改成调用 `traffic_gate_->onFeedback(...)`。`robot_pose` 只有 1 Hz，**禁止**用作进出判据；`plan` 订阅已移除。

### 5.4 `emitRequest()` —— 统一出口

```cpp
void TrafficGate::emitRequest(GateContext& g, const std::string& op)
{
    TrafficRequest m;
    m.stamp          = now();
    m.op             = op;                       // REQUEST | CANCEL | RELEASE
    m.request_id     = g.request_id;             // 同一 gate 全程复用，幂等
    m.robot_id       = robot_id_;
    m.resource_id    = g.resource_id;
    m.epoch          = g.epoch;
    m.eta_sec        = (op == "REQUEST") ? estimateEta(g) : 0.0;
    m.lease_ttl_sec  = cfg_.lease_ttl_sec;       // [TBD-020 建议默认 60.0]
    req_pub_->publish(m);                        // ROS2 traffic_request → amr_agent
}
```

`request_id` 生成建议（非冻结）：`{robot_id}-{resource_id}-{task_seq}`，在 `IDLE → REQUESTING` 时生成一次并缓存。

### 5.5 `onResponse()` —— 分状态处理

```cpp
void TrafficGate::onResponse(const TrafficResponse& r)
{
    auto it = gates_.find(r.resource_id);
    if (it == gates_.end()) { log("stale_response_unknown_resource"); return; }
    auto& g = it->second;
    if (r.request_id != g.request_id) { log("response_id_mismatch"); return; }
    if (r.epoch < g.epoch)            { log("stale_epoch_dropped", r.epoch, g.epoch); return; }

    switch (hash(r.result)) {
    case "GRANTED":
        if (g.state == GateState::REQUESTING) {
            // ★ 正常路径：零动作。不减速、不 cancel、不重发 goal
            g.state = GateState::HELD;
            log("early_granted_zero_action", r.resource_id);
        } else if (g.state == GateState::WAITING) {
            // 事件驱动：立即重发 D，不等 10 s 轮询
            g.epoch++;
            nav_->navigateToPose(final_goal_D_);
            g.state = GateState::RESUMING;
            log("resume_to_D", r.resource_id);
        }
        break;

    case "REJECTED": case "PENDING":
        if (g.state == GateState::REQUESTING) onRejected(g);
        // WAITING 状态下 PENDING 不改状态，继续 10 s 轮询
        break;

    case "EXPIRED":
        // Edge 已回收；若车仍在窄道附近，必须重新申请，不得静默续租
        if (g.state == GateState::HELD || g.state == GateState::RESUMING) {
            log("lease_expired_while_held", r.resource_id);
            onRejected(g);                       // 保守：退到等待点重新申请
        }
        break;

    case "CANCELLED": case "RELEASED":
        g.state = GateState::RELEASED;
        break;
    }
}
```

### 5.6 `onRejected()` —— cancel → 等 result → 等待点 → WAITING

```cpp
void TrafficGate::onRejected(GateContext& g)
{
    // ① 取消当前导航（进程内调用，不新增 ROS2 接口）
    g.state = GateState::CANCELING;
    nav_->cancelNavigation();                 // NavigationClient::cancelNavigation()

    // ② 关键：cancel 是异步的。必须等 result 回调把 m_goal_active 置回 false，
    //    否则 navigateToPose() 会直接拒绝新 goal（navigation_client.cpp:28-32）
    //    实现：在 NavigationClient 的 result 回调里调用 onNavResult(CANCELED, g)
    //    不得用 sleep / 定时猜测。
}

void TrafficGate::onNavResult(NavResult res, GateContext& g)
{
    if (g.state != GateState::CANCELING) return;
    if (res != NavResult::CANCELED && res != NavResult::ABORTED) return;
    // m_goal_active 已为 false，此时才能发新 goal
    nav_->navigateToPose(g.wait_pose);        // 进入侧 wait_poses[entry_side] 组内 argmin；入口端边外侧（上游），已锁存
    g.state = GateState::CANCELING;           // 到点回调里转 WAITING
}

void TrafficGate::onWaitPointArrived(GateContext& g)
{
    g.state = GateState::WAITING;
    g.last_request_at = now();
    // WAITING：Nav2 无活动 goal（无规划输出）、cmd_vel 闸门归零
    // 结构上规避 SimpleProgressChecker 的 movement_time_allowance = 20 s
}
```

**三个必踩的坑**：① `m_goal_active` 忙则拒绝新 goal；② `navigation_server.cpp:307` 无条件 `ACCEPT_AND_EXECUTE` 的 supersede 可能与显式 cancel 叠加；③ 60 s 结果超时只 abort 不 cancel Nav2 goal（`:456-470`）。这三个都在 §10 的前置阻塞清单里，**必须先修**。

### 5.7 固定 10 s 幂等重发

```cpp
void TrafficGate::onTick()                    // 由 BT tick 或 1 s 定时器驱动
{
    for (auto& [rid, g] : gates_) {
        if (g.state != GateState::WAITING) continue;
        if (now() - g.last_request_at < std::chrono::seconds(10)) continue;  // T_retry = 10 s，固定无 jitter
        emitRequest(g, "REQUEST");            // 同一 request_id，Edge 幂等去重
        g.last_request_at = now();
        log("retry_request", rid, g.request_id);
    }
}
```

> 主路径是**事件驱动**（`onResponse()` 收到 `GRANTED` 立即重发 D），10 s 轮询只作丢包 / 拒答兜底。固定 10 s 带来的多车同步申请由 Edge FIFO + 幂等吸收，**不加 jitter**。

### 5.8 四触发释放（取最早者）

```cpp
void TrafficGate::maybeRelease(GateContext& g, ReleaseTrigger t)
{
    if (g.state == GateState::RELEASED) return;
    emitRequest(g, "RELEASE");
    g.state = GateState::RELEASED;
    g.already_handled = true;
    log("release", g.resource_id, triggerName(t));
}
// 触发来源（四触发取最早者，V0.20）：
//   ① 出区边沿 onFeedback() inside:true->false（含滞回）  —— 主判据
//   ② onTaskFinished(SUCCEEDED/ABORTED)                     —— 兜底
//   ③ onTaskCancelled() 显式 op=CANCEL                      —— 兜底
//   ④ Edge 侧 lease_ttl 到期 EXPIRED（AMR 不主动发）        —— 最终兜底
// 注：改道不设独立触发（旧 onPlanChanged() 已移除）；驶离控制区归 ①、停区内 / 位姿断流归 ④，见 §5.9
```

### 5.9 改道释放语义：由出区边沿 + TTL 承接（V0.20 定案 6 / 7，`onPlanChanged()` 已整节移除）

> **本节不再有伪代码**。`onPlanChanged()`、`pathIntersects`、`pathIntersectsWithin`、`arcLengthToGate`、`reroute_streak_` / `reroute_debounce_N_`、`prev_dist_to_gate_`、`distanceToGate` 等改道去抖机制**全部移除**（已废弃）；`plan` 订阅已省，改道检测失去数据源。

**「改道即释放」保留语义、移除独立机制**（定案 6 + 7 推导结论）。它要防的是「授权后车不再经过窄道却不释放」；在中心点**出区即释放**的主判据下，已被既有机制完整承接：

| 改道后的情形 | 由谁释放 | 依据 |
| --- | --- | --- |
| Nav2 改选绕行路，车驶离通行控制区 | **出区边沿** `inside true→false` → `op=RELEASE` | 定案 1；区域贴窄道且端边不压分叉点 ⇒ 改道必然走出区域 |
| 车停区内不动 / 位姿断流 / 崩溃 | **`lease_ttl` 到期 `EXPIRED`** | 定案 7 保留 |
| 任务 `SUCCEEDED` / `ABORTED` / 显式 `CANCEL` | §5.8 四触发取最早 | 定案 7 保留 |

- 旧口径「**改道即释放不可省**」强调的是**语义不可省**、不是 `onPlanChanged()` 这个**实现**；语义仍不可省，但由出区边沿 / TTL 覆盖，不设独立机制（§11 自检同步改写）。
- `zonePolygon()` **保留**：validator 校验、异常裁决、rviz 可视化仍用它，只是**不再被改道检测调用**（旧 `pathIntersects(path, zonePolygon(rid))` 调用已移除）。
- 兜底一个不省（定案 7）：固定 `T_retry=10 s` 同 `request_id` 幂等重发、Edge FIFO + `PENDING` + `tryGrant()`、`lease_ttl` / `EXPIRED`、`epoch` 丢迟到消息、显式 `op=CANCEL`、四触发取最早、`m_nav_epoch` 竞态修复、AC-019 ~ AC-025 全部保留。

### 5.10 任务结束 / 取消

```cpp
void TrafficGate::onTaskFinished(NavResult result)
{
    for (auto& [rid, g] : gates_)
        if (g.state != GateState::RELEASED) maybeRelease(g, ReleaseTrigger::TASK_END);
}

void TrafficGate::onTaskCancelled()
{
    for (auto& [rid, g] : gates_) {
        if (g.state == GateState::RELEASED) continue;
        emitRequest(g, "CANCEL");             // 排队中出队 / 已授权释放
        maybeRelease(g, ReleaseTrigger::TASK_CANCEL);
    }
}
```

---

## 6. Edge 侧仲裁伪代码

### 6.1 `AmrCommunication` 消息分发

```cpp
void onMqttMessage(const std::string& topic, const std::string& payload)
{
    // topic = amr/+/traffic_rights/request
    std::string topic_robot = extractRobotId(topic, /*level=*/1);
    Json j = parse(payload);
    if (!j.contains("op"))            { log("drop_missing_op");        return; }
    if (j["op"] != "REQUEST" &&
        j["op"] != "CANCEL"  &&
        j["op"] != "RELEASE")        { log("drop_invalid_op");        return; }
    if (j["robot_id"] != topic_robot){ log("drop_robot_id_mismatch"); return; }   // 防串台 / 伪造

    TrafficRequest req = decode(j);   // §2.1 字段
    switch (hash(req.op)) {
    case "REQUEST": service_->onRequest(req);  break;
    case "CANCEL":  service_->onCancel(req);   break;
    case "RELEASE": service_->onRelease(req);  break;
    }
    // 单播响应，绝不广播
}
```

### 6.2 `onRequest()` —— 幂等 → PENDING → FIFO 授权

```cpp
void TrafficRightsService::onRequest(const TrafficRequest& r)
{
    // ① 幂等：相同 request_id 直接返回原结果，不产生第二份 lease
    if (auto old = db_->findByRequestId(r.request_id)) {
        publishResponse(makeResponse(*old));   // 重传 / 10 s 重发都走这里
        return;
    }
    // ② 陈旧消息：epoch 小于当前代次直接丢弃
    if (r.epoch < db_->currentEpoch(r.robot_id, r.resource_id)) {
        log("drop_stale_epoch", r.request_id, r.epoch);
        return;
    }
    // ③ 入队（PENDING 不占资源，可共存）
    Lease lease = makeLease(r, /*state=*/"PENDING",
                            /*queue_seq=*/db_->nextQueueSeq(r.resource_id));
    db_->insert(lease);
    publishResponse(makeResponse(lease));       // 先回 PENDING，AMR 继续沿原 goal 行驶

    // ④ 尝试按 FIFO 授权
    tryGrant(r.resource_id);
}

void ResourceBroker::tryGrant(const std::string& resource_id)
{
    // 原子授权：同一 (resource_type, resource_id) 只能有一个 GRANTED/HELD
    // 部分唯一索引 idx_lease_active 保证并发安全（interface-contract.md §8.6）
    auto head = db_->oldestPending(resource_id);          // ORDER BY queue_seq LIMIT 1
    if (!head) return;
    if (db_->hasActiveHolder(resource_id)) return;        // 有人占着，队首继续等

    auto now = simNow();
    bool ok = db_->atomicUpdate(
        "UPDATE tbl_resource_lease SET state='GRANTED', granted_at=?, "
        "lease_expires_at = ? + lease_ttl_sec "
        "WHERE request_id=? AND state='PENDING'", now, now, head->request_id);
    if (!ok) return;                                      // 并发下被别的线程抢先，下次定时器再试

    publishResponse(makeResponse(db_->findByRequestId(head->request_id)));  // GRANTED
    log("granted", resource_id, head->request_id, head->queue_seq);
}
```

> **授权生效时刻**：`lease_expires_at = granted_at + lease_ttl_sec`。`eta_sec` 只供排队参考，不做「按 ETA 插队」（复杂度边界）。`lease_ttl_sec` 默认值属 TBD-020。

### 6.3 `onCancel()` / `onRelease()` —— 两态语义

```cpp
void TrafficRightsService::onCancel(const TrafficRequest& r)
{
    auto lease = db_->findByRequestId(r.request_id);
    if (!lease) { log("cancel_unknown_request"); return; }
    if (r.epoch < db_->currentEpoch(r.robot_id, r.resource_id)) { log("drop_stale_cancel"); return; }

    if (lease->state == "PENDING") {
        db_->updateState(r.request_id, "CANCELLED");      // 排队中 → 出队，不影响已授权的车
        log("cancel_dequeue", r.request_id);
    } else if (lease->state == "GRANTED" || lease->state == "HELD") {
        db_->updateState(r.request_id, "RELEASED");       // 已授权 → 释放 lease
        rollbackQuota(r);                                 // 配额回退，与 AC-011 语义一致
        log("cancel_release", r.request_id);
        tryGrant(r.resource_id);                          // 立刻让队首接上
    }
    db_->bumpEpoch(r.robot_id, r.resource_id, r.epoch);
    publishResponse(makeResponse(db_->findByRequestId(r.request_id)));   // CANCELLED / RELEASED
}

void TrafficRightsService::onRelease(const TrafficRequest& r)
{
    auto lease = db_->findByRequestId(r.request_id);
    if (!lease || lease->state == "RELEASED") return;    // 幂等
    db_->updateState(r.request_id, "RELEASED");
    log("released", r.resource_id, r.request_id);
    tryGrant(r.resource_id);
    publishResponse(makeResponse(*lease));               // RELEASED
}
```

### 6.4 TTL 定时器（最终兜底）

```cpp
void ResourceBroker::onTick1s()
{
    // ① lease_ttl 到期：AMR 崩溃 / cancel 丢包 / 改道后停区内不动 / 位姿断流 / 通信中断
    for (auto& l : db_->expiredLeases(simNow())) {
        db_->updateState(l.request_id, "EXPIRED");
        log("lease_expired", l.resource_id, l.request_id);
        publishResponse(makeResponse(db_->findByRequestId(l.request_id)));  // EXPIRED（尽力而为）
        tryGrant(l.resource_id);
    }
    // ② 心跳超时（V1 为 5 分钟）/ Last Will 触发 → 同样回收
    for (auto& l : db_->leasesOfOfflineRobots()) {
        db_->updateState(l.request_id, "EXPIRED");
        tryGrant(l.resource_id);
    }
    // ③ PENDING 超时作废 [TBD-020]；AMR 侧重发会生成新排队项
    // ④ 每个资源尝试给队首授权（防止 6.2 并发漏掉）
    for (auto& rid : db_->resourcesWithPending()) tryGrant(rid);
}
```

> **「怎么知道 AMR 取消了申请」的最终答案**：**不必知道**。快路径是显式 `CANCEL`，安全网是 `lease_ttl` 到期自动回收。两者是同一个机制的两面。

### 6.5 重启恢复

```cpp
void ResourceBroker::onStartup()
{
    // 从 SQLite 重建占用状态：GRANTED/HELD 的 lease 保留并继续计时
    // PENDING 队列：一并重建，或显式作废并记日志（不得留下幽灵排队项）
    db_->rebuildEpochFromLeases();
    for (auto& rid : db_->resourcesWithPending()) tryGrant(rid);
}
```

### 6.6 `amr_power_manager`（对比用，确实简单）

```cpp
void PowerManager::onPowerMode(const PowerMode& m) { mode_ = m.mode; mode_stamp_ = now(); }
void PowerManager::onTick(double dt_sim_sec)
{
    double rate = (mode_ == CHARGING) ? +cfg_.charge_pct_per_min
                : (mode_ == WORK)     ? -cfg_.work_pct_per_min
                                      : -cfg_.idle_pct_per_min;       // [TBD-022]
    soc_ = clamp(soc_ + rate * dt_sim_sec / 60.0, 0.0, 100.0);
    BatteryState b; b.battery = soc_;
    b.is_low = soc_ <= cfg_.low_soc_threshold;                        // [TBD-022]
    pub_->publish(b);                                                  // power_level
}
```

一个订阅、一个积分定时器、一个发布；零判定、零 `cmd_vel` 订阅、零跨进程控制链。`robot_status → amr_agent → Edge` 上报链路 V1 已通，只需移除两处 `100.0` 硬编码。

---

## 7. 全场景功能流程（对照联调与验收）

> 所有场景 `task_state` 全程不变（INV-13）。泳道与箭头见 `docs/assets/v2-traffic-sequence.svg`。

### S1 正常路径：进区申请 → 提前授权 → 零动作（最常见）

V0.20：**不再有「`plan` 提前预判」这一跳**，提前量由拉长区域几何自带（§5.2）；申请由中心点进区边沿触发。

| # | 谁 | 动作 | 接口 |
| --- | --- | --- | --- |
| 1 | behavior_manager | `GoodsPointResolver(goods_id, task_type)` → 目标 D；`navigateToPose(D)` | `amr_navigate_to_pose` |
| 2 | `TrafficGate::onFeedback()` | 中心点进区边沿（`inside` `false→true`，含滞回）→ 锁存 `entry_side` / 选 `wait_pose` → `emitRequest(op=REQUEST)`，标 `REQUESTING`（**不动 goal**） | `traffic_request` |
| 3 | `TrafficBridge` | 原样转 MQTT（不改字段） | `amr/{id}/traffic_rights/request` |
| 4 | Edge | 订阅校验 → `onRequest()` → 入 `PENDING` → `tryGrant()` 原子授权 | `amr/+/traffic_rights/request` |
| 5 | Edge | 单播 `GRANTED`，写 `lease_expires_at` | `amr/{id}/traffic_rights/response` |
| 6 | `TrafficGate::onResponse()` | **`REQUESTING → HELD`，零动作**：不减速、不 cancel、不重发 goal（AC-020） | — |
| 7 | 车 | 沿原 `NavigateToPose(D)` 直接通过窄道 | — |

**验收**：AC-020「授权提前到达零 goal 重发、零停顿」。

### S2 异常路径：被拒 → 等待点 → 10 s 重发 → 重发 D

| # | 谁 | 动作 |
| --- | --- | --- |
| 1 | Edge | `REJECTED` / `PENDING`（队首被别的车占着） |
| 2 | `TrafficGate::onRejected()` | `nav_->cancelNavigation()`，状态 `CANCELING` |
| 3 | `NavigationClient` result 回调 | `m_goal_active=false` → 调 `onNavResult(CANCELED, g)` |
| 4 | `TrafficGate` | `navigateToPose(wait_pose)`（进入侧 `wait_poses[entry_side]` 组内 `argmin`，入口端边外侧上游，已锁存） |
| 5 | `onWaitPointArrived()` | `WAITING`：Nav2 无活动 goal（无规划输出）、`cmd_vel` 归零 |
| 6 | `onTick()` | 每固定 10 s 用**同一 `request_id`** 幂等重发 `REQUEST` |
| 7 | Edge | 队首释放后 `tryGrant()` 授权，单播 `GRANTED` |
| 8 | `onResponse()` | **事件驱动立即** `navigateToPose(D)`，不等轮询；状态 `RESUMING → HELD` |

**验收**：AC-019 / AC-020；等待期间 `ros2 node info` 可核对「无活动 goal」。

### S3 正常释放：中心点出区边沿（V0.20）

| # | 谁 | 动作 |
| --- | --- | --- |
| 1 | Nav2 | action feedback `current_pose`（填实 V1 空回调），取中心点 `inside` |
| 2 | `onFeedback()` | 出区边沿 `inside` `true→false`（滞回外扩阈值）→ `emitRequest(op=RELEASE)` |
| 3 | `TrafficBridge` → Edge | `onRelease()` → `HELD → RELEASED` → `tryGrant(队首)` |
| 4 | 车 | **不停、不改 goal**，继续 `NavigateToPose(D)` |

**验收**：AC-022；端边外移 `min_release_clearance` 保证「中心出区 ⇒ 车尾已出真实窄道」，不做逐帧 `footprint`。

### S4 改道场景：由出区边沿 / TTL 承接（V0.20，无独立改道机制）

| # | 谁 | 动作 |
| --- | --- | --- |
| 1 | Nav2 | 授权后改选绕行路，车驶离通行控制区（区域贴窄道、端边不压分叉点 ⇒ 改道必然走出区域） |
| 2 | `onFeedback()` | 中心点出区边沿 `inside true→false` → `emitRequest(op=RELEASE)`（§5.3 主判据，非 `onPlanChanged()`） |
| 3 | `TrafficBridge` → Edge | `onRelease()` → `RELEASED` → `tryGrant()`，队首对向车立即获批 |
| 4 | 兜底 | 若车停区内不动 / 位姿断流 → `lease_ttl` 到期 `EXPIRED` 自动回收（§5.9 / §6.4） |

**验收**：AC-021。「改道即释放」**语义保留**但**无独立机制**（旧 `onPlanChanged()` 已移除）：驶离由出区边沿覆盖、停住由 TTL 覆盖；语义不可省，实现不另起一套去抖。

### S5 兜底：`SUCCEEDED` / TTL / 显式取消

| 触发 | 链路 | 说明 |
| --- | --- | --- |
| 导航 `SUCCEEDED` / 任务结束 / `ABORTED` | `onTaskFinished()` → `op=RELEASE` | 出区边沿漏检时兜底，**已降级为兜底不是主判据** |
| `lease_expires_at` 到期 | Edge `onTick1s()` → `EXPIRED` → 自动回收 | AMR 崩溃 / `cancel` 丢包 / 通信中断 / 车停区内不动 / 位姿断流 |
| AMR 显式 `op=CANCEL` | `onCancel()` | `PENDING` 出队；`GRANTED`/`HELD` 释放 + 配额回退；旧 `epoch` 丢弃 |
| QoS 1 重传 | `request_id` 幂等 | 直接返回原结果，不产生第二份 lease |

### S6 走绕行路（零申请、零停顿）

Nav2 从一开始就选了不含窄道的绕行路；因通行控制区端边**不得压到分叉点之前**（§5.2 几何 / §3.3 validator 第 ⑥ 条），该车**从不进入通行控制区**（中心点 `inside` 全程为 false）→ 无进区边沿 → **零 `TrafficRequest`、零停顿**。系统不需要知道有几条路线，也不需要预先决定走哪条；这正是 INV-13「保留 Nav2 动态选路」的验收证据。旧「从未跨过任何触发段 / `onPlan()` 的 `pathIntersectsWithin()` 全为 false」判据**已废弃**，S6 判据改为「从不进区」。

### S7 两车对向争用同一窄道

| # | A 车 | B 车 | Edge |
| --- | --- | --- | --- |
| 1 | 发 `REQUEST`（`queue_seq=1`） | 发 `REQUEST`（`queue_seq=2`） | FIFO 入队 |
| 2 | `GRANTED`，`HELD`，零动作通过 | `PENDING` → 收到 `REJECTED` | 原子授权 A |
| 3 | 出区边沿 → `RELEASE` | `WAITING`，10 s 幂等重发 | `RELEASED` → `tryGrant()` |
| 4 | — | `GRANTED` → 立即重发 D | 授权 B |

**验收**：AC-019「同一时刻只有一个 holder、不串台」；AC-023「显式 cancel 两态、TTL 回收、旧 epoch 丢弃」。

---

## 8. 配置项清单

集中放在 `edge_server/config/edge_server.yaml`（Edge 侧）与 AMR 侧对应参数文件。**标 `[TBD]` 的是建议默认值，仅用于本地调试，拍板前不得当作冻结接口。**

| 配置 | 建议默认 | 归属 | 说明 |
| --- | --- | --- | --- |
| `traffic.retry_interval_sec` | `10.0` | AMR | **已冻结**：固定 `T_retry = 10 s`，无 jitter |
| `traffic.lease_ttl_sec` | `60.0` | AMR + Edge | `[TBD-020]` 权限占用时限；申请时带，授权时写入 `lease_expires_at` |
| `traffic.pending_timeout_sec` | `120.0` | Edge | `[TBD-020]` `PENDING` 超时作废 |
| `traffic.queue_seq_source` | `AUTOINCREMENT` | Edge | `[TBD-020]` FIFO 序号生成方式 |
| `traffic.L_pull_m` | `1.0` | 地图 | `[TBD-021/038]` 通行控制区侧边沿通道拉长量，自带提前量；V1.5 基准实验实测后确定（原 `D_trigger_m` 已废弃，转译为本项） |
| `traffic.d_wait_to_gate_m` | `0.6` | 地图 | `[TBD-021]` 等待点到入口端边外侧（上游）净距 |
| `traffic.min_release_clearance_m` | `0.8` | 地图 | `[TBD-021]` 端边外移量 = 车长/2 + `xy_goal_tolerance 0.15` + yaw 投影余量；保证中心出区 ⇒ 车尾已出真实窄道 |
| `traffic.gates_yaml_source` | `MapInfo.traffic_gates_yaml` | AMR | `[TBD-039]` 新 `traffic_gate_loader` 从 `MapInfo.msg` 读 4 点 `polygon` + `wait_poses{l1,l2}` 的落地形态（原 `crossed()` 朝向阈值口径已废弃） |
| `traffic.hysteresis_m` | `0.05` | AMR | `[TBD-038]` 中心点进出滞回双阈值（内缩进入 / 外扩离开）+ 可选连续 N 帧时间窗去抖（原 `reroute_debounce_N` 改道去抖已废弃） |
| `power.idle_pct_per_min` | `-0.5` | AMR | `[TBD-022]` IDLE 档消耗速率 |
| `power.work_pct_per_min` | `-2.0` | AMR | `[TBD-022]` WORK 档消耗速率 |
| `power.charge_pct_per_min` | `+5.0` | AMR | `[TBD-022]` 充电速率（B 方案时间积分） |
| `power.low_soc_threshold` | `20.0` | AMR | `[TBD-022]` 低电量准入阈值 |
| `power.charge_target_soc` | `90.0` | AMR | `[TBD-022]` 停止充电阈值 |
| `power.mode_timeout_sec` | `5.0` | AMR | `[TBD-022]` `PowerMode` 超时回落 IDLE |

Nav2 侧**不改**：`movement_time_allowance: 20.0`（`nav2_params.yaml:125-128`）、`max_vel_x: 0.26`（`:146`）、`decel_lim_x: -0.5`（`:157`）、`xy_goal_tolerance: 0.15`、`yaw_goal_tolerance: 0.2`。等待时「Nav2 无活动 goal」是结构选择，不靠调参规避 20 s 超时。

---

## 9. 错误 / 竞态 / 边界处理表

| 场景 | 处理 | 依据 |
| --- | --- | --- |
| `cancelNavigation()` 后立即发新 goal 被静默丢弃 | 必须等 result 回调把 `m_goal_active` 置回 false；用显式 `CANCELING` 状态等待，**禁止 `sleep` 猜测** | `navigation_client.cpp:28-32` |
| 显式 cancel 与 `navigation_server` 内部 supersede 叠加成两次 cancel | 先修 §10 缺陷 1；`TrafficGate` 用 `epoch` 去重 | V2 分册 3.2.3 |
| 迟到响应（旧 `epoch`） | AMR 与 Edge 双侧都丢弃并记录，不回退状态 | D-062 |
| QoS 1 重传 / 10 s 幂等重发 | `request_id` 去重，返回原结果，不产生第二份 lease | D-062 |
| AMR 崩溃 / `cancel` 丢包 / 断网 | Edge `lease_ttl` 到期自动回收 `EXPIRED` | D-062 / AC-023 |
| 改道后驶离通行控制区 / 停区内不动 | 驶离由「出区边沿」释放，停区内 / 位姿断流由 `lease_ttl` / `EXPIRED` 回收（V0.20 无独立改道机制，§5.9） | D-061 / AC-021 |
| 出区边沿漏检 | `SUCCEEDED` / 任务结束 / `ABORTED` 兜底释放 | D-061 |
| 中心点在区域边界抖动导致重复 `REQUEST` / `RELEASE` | `inside` 边沿检测 + 滞回双阈值（内缩进入 / 外扩离开）+ 可选连续 N 帧一致（阈值 `[TBD-038]`） | §5.1 / §5.3 |
| 车走到区域中部 `s≈0` 时被拒 | `entry_side` 本次穿越周期**锁存、绝不每帧重算**，等待点始终取进入侧 `wait_poses[entry_side]` 一组 | 定案 3 / §5.3 |
| 多车同步 10 s 申请 | Edge FIFO + 幂等吸收，**不加 jitter** | V2 分册 3.2.2 |
| Edge 重启 | 从 SQLite 重建 `GRANTED`/`HELD` 与 `PENDING`（或显式作废），不得留幽灵排队项 | D-062 |
| `PENDING` 长期未获批 | `[TBD-020]` 超时作废，AMR 侧重发生成新排队项 | V2 分册 3.6 |
| 等待点落在区域内 / 未在上游 / 等待路径穿真实窄道 | `validator.py` 7 条内容校验拦截（等待点在端边外侧、同侧间距 ≥ 车宽 + `d_queue_spacing`） | §3.3（7 条校验全文） |
| 4 点 `polygon` 不凸 / 点序不连续 / 端边压到分叉点之前 / 与 `walls.yaml` 冲突 | `validator.py` 7 条内容校验拦截（保 S6 走绕行路零申请） | §3.3（7 条校验全文） |
| `robot_id` 与 topic 不一致 | 丢弃并记日志，防串台 / 伪造 | §6.1 |
| 授权后导航失败 | BT `Retry` / `Fallback`，并释放路权，避免占着路口不动 | V2 分册 3.2.3 |
| 持权期间任务被取消 | 释放路权 + 配额回退，与 AC-011 语义一致 | V2 分册 3.2.4 |

---

## 10. 前置阻塞：`amr_navigation` 的 4 个缺陷（接入路权前必须先修）

> V0.20：这 4 项仍为前置阻塞，其中缺陷 3 的 `m_nav_epoch` 竞态修复属定案 7 明确保留的兜底。中心点进出区域的新判据**不新增任何 ROS2 导航接口**（§1.1 / §2.5），故本节修复范围不变。

| # | 缺陷 | 源码坐标 | 修复要点 |
| --- | --- | --- | --- |
| 1 | 对所有 goal 无条件 `ACCEPT_AND_EXECUTE`，busy 时内部 supersede 再 cancel 旧 goal 重发 | `navigation_server.cpp:290-308`、`:343-356` | busy 时拒新 goal 或走显式 cancel 流程；避免与 `TrafficGate` 的 cancel 叠加 |
| 2 | 60 s 结果超时只 abort、不 cancel Nav2 goal | `navigation_server.cpp:456-470`、`:381-389` | 超时必须 `async_cancel_goal()` 并等 result，否则「cancel → 重发」白等 60 s |
| 3 | `m_nav_epoch` 非 atomic、写于结果回调之后，无锁共享状态 | `navigation_server.cpp:344` / `:372` / `:442` / `:831` | 改 `std::atomic` 或加锁；与 Edge 侧 `epoch` 防陈旧同一套思路 |
| 4 | `accepted` 语义混淆（`async_send_goal` 后立即置 true） | `navigation_manager.cpp:58-65`、`:68-85` | 改名为 `submitted`；真实接受以 `goalResponseCallback` 为准 |

> 这 4 项在 V2 分册 3.2.3 已列为**阻塞项**。控制链 `§5` 给了完整调用链：`NavigationClient::cancelNavigation()`（`navigation_client.cpp:81-102`）→ `NavigationServer::handleCancel()`（`:310-318`）→ `NavigationManager::cancelNavigation()`（`navigation_manager.cpp:152-160`）→ Nav2 原生 `async_cancel_goal()` → result `CANCELED` → `m_goal_active=false`。**不新增任何 ROS2 导航接口**。

---

## 11. 实现顺序与自检清单

### 11.1 顺序（V2 分册 3.7 的实现级展开）

1. **读文档**：SRS V0.20 §4.10/4.11 → 本文 §0~§3 → V0.20 8 项定案记录（`.workbuddy/artifacts/traffic-gate-8-decisions-frozen.md`）→ `v2-traffic-control-chain.md` §4 → V2 分册 3.2。
2. **修 4 个导航缺陷**（§10），单独回归 V1 的 AC-009 / AC-011，确保没打破 V1 闭环。
3. **建接口**：4 个 `.msg` + 4 个 ROS2 常量 + 3 个 MQTT 常量 + 2 个 helper；`colcon build` 通过。
4. **补 `TrafficBridge`**：只做透明桥，先跑通「`traffic_request` 发出 → MQTT 收到」「MQTT 造一条 response → `traffic_response` 收到」的环回测试。
5. **地图几何**：新建独立 `traffic_gates.yaml`（4 点 `polygon` + `wait_poses{l1,l2}`）、`MapInfo.msg` 加 `traffic_gates_yaml`、打通 §3.3 的 8 处打包下发链路；`validator.py` 加 7 条内容校验；新 `traffic_gate_loader` 读几何。**先用脚本验证**：凸四边形 / 点序连续、端边不压分叉点（保 S6 零申请）、等待点在入口端边外侧且同侧间距 ≥ 车宽 + `d_queue_spacing`、`entry_side` 锁存不翻侧。
6. **Edge 侧**：`tbl_resource_lease`（建议 DDL）+ `ResourceBroker` + `traffic_rights_service`；先跑通 S7「两车争用只有一个获批」「显式 cancel 两态」「TTL 回收」，不接 AMR 也能单测。
7. **AMR 侧 `TrafficGate`**：按 §4/§5 实现，先接 S1（进区申请 → 提前授权零动作）、再 S2（被拒等待、进入侧锁存选点）、S3（中心点出区边沿释放）、S4（改道由出区边沿 / TTL 承接，无独立机制）。
8. **电量与 BT**：`amr_power_manager`（§6.6）+ `PowerMode` 发布；BT 节点按 TBD-023 落地。
9. **急停**：按 TBD-024 落地 `cmd_vel` 仲裁。
10. **联调验收**：AC-019 ~ AC-025；提交说明附「为什么这么实现」。

### 11.2 自检清单（提测前逐条勾）

- [ ] 全仓**无**已废弃的独立交通模块命名；`amr_behavior_manager` 包内存在 `TrafficGate`，`amr_agent` 包内存在 `TrafficBridge`，**没有第三个新 ROS2 包**（`amr_power_manager` 除外）。
- [ ] `amr_common::topic` 里 4 个常量齐全；`mqtt_topic.h` 里旧 `TRAFFIC_RIGHTS_*` 已替换。
- [ ] `task_command` / `task_event` 的 `.msg` 与桥接代码**一行未改**。
- [ ] `current_pose` 从 action feedback 取（V1 空回调已填实）；主判据为中心点 `point_in_poly` 的 `inside` 边沿 + 滞回双阈值；**无 `plan` 订阅**（V0.20 已废弃）；`robot_pose` 未被用作进出判据。
- [ ] `NavigationClient::cancelNavigation()` 直接复用；**等 result 回调 `m_goal_active=false`** 后才发等待点 / D。
- [ ] 目标全程为 D；`REQUESTING` 收 `GRANTED` **零 goal 重发、零停顿**（AC-020），仅 `WAITING` 获批才立即重发 D；等待点是唯一例外，取进入侧 `wait_poses[entry_side]` 组内 `argmin` 且位于入口端边外侧（上游），`entry_side` 本次穿越周期锁存。
- [ ] 等待期间 Nav2 无活动 goal（无规划输出）；固定 10 s 无 jitter、同 `request_id` 幂等；`WAITING` 收到 `GRANTED` 事件驱动立即重发 D（`REQUESTING` 零动作）。
- [ ] 释放四触发取最早（出区边沿 / 任务结束 / 显式 `CANCEL` / TTL）；**改道场景由出区边沿 / TTL 覆盖**（语义不可省、无独立 `onPlanChanged()` 机制）；`SUCCEEDED` 只是兜底。
- [ ] 申请带 `lease_ttl_sec`；Edge `PENDING` 不进部分唯一索引；显式 `CANCEL` 分两态；`epoch` 丢弃迟到消息；占用状态不广播。
- [ ] Edge 只订阅 `amr/+/traffic_rights/request`；`robot_id` 与 topic 交叉校验；`op` 缺失即丢弃。
- [ ] `NavigateThroughPoses`、独立 `ComputePath` / `FollowPath` Action Client **均未被引入**；Nav2 `plan` **无订阅**（V0.20）。
- [ ] `task_state` 全程不变，无 `INTERRUPTED`、无重复 `CARRY_*` 上报。
- [ ] 4 个导航缺陷已修并回归 V1 的 AC-009 / AC-011。
- [ ] 所有 `[TBD]` 数值集中在 §8 配置表，代码里没有散落的魔法数字。

---

## 12. 仍待冻结（实现者不得自行选择）

| 项 | 归属 | 本文的处理 |
| --- | --- | --- |
| `queue_seq` 生成、`PENDING` 超时、`lease_ttl` 默认值与续租规则、表最终字段 / 是否新增 `tbl_resource_lease` | TBD-020 | §6 / §8 给建议默认，仅用于本地调试 |
| 区域 / 数据字段名（`polygon` / `wait_poses{l1,l2}`，无 `side` / `forward`）、`resource_id` 取值、`L_pull` / `min_release_clearance` / `d_wait_to_gate` 数值 | TBD-021 | §3.3 / §8；数值由 V1.5 基准实验实测（`D_trigger` 已废弃，转译 `L_pull`） |
| 中心点进出滞回去抖阈值（内缩 / 外扩，`[TBD-038]`）、新 `traffic_gate_loader` 读取落地（`[TBD-039]`）、ETA 精度与 lease 生效时刻 | TBD-038 / 039 | §5.1 / §3.1 / §3.3 / §8 |
| 电量三个速率、`mode_timeout_sec`、双阈值、是否新增 `tbl_robot_battery_log` | TBD-022 | §6.6 / §8 |
| BT.CPP 版本、XML 路径、自定义节点注册、重试次数 | TBD-023 | §3.1 只列文件，不写死实现 |
| 软件急停触发源与 `cmd_vel` 仲裁落点 | TBD-024 | §11 顺序第 9 步，未定形态 |
| 地图包版本升级与 TBD-003 的关系、ADR-0002、V1.5 自验 | 流程前置 | 不在本文范围 |

> **本文定位**：把「已冻结的决策」翻译成「可照着写代码的字段、状态机、伪代码与调用顺序」。它不替代 SRS / V2 分册，也不替项目所有者拍板 §12 的各项。若本文与上游冲突，**以上游为准并回报冲突**。

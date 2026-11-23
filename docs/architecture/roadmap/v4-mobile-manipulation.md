# V4 车顶机械臂（移动操作） · 开发任务书

**Status:** Planned
**所属路线图:** [`../version-roadmap.md`](../version-roadmap.md)
**版本主题:** 在 AMR 上加装机械臂，实现「停稳后抓取」，替换行为树的 `Grasp` 占位叶节点
**前置依赖:** V3 完成（货箱模型与 attach/detach 已可用）、V2 完成（BT 骨架、急停与安全边界已就位）

---

## 1. 目标

在 AMR 本体上增加机械臂与夹爪，把 V2 行为树中 `Grasp` / `Place` 两个**占位叶节点**替换为真实的抓取动作实现，使货物从「随车挂载」升级为「被机械臂拿起/放下」，补齐感知—决策—执行闭环中的执行环节。

> **版本决策依据**：项目所有者倾向将机械臂装在 AMR 上（移动操作 / mobile manipulation），而非工位侧固定臂。该选择更贴近具身智能方向、演示画面更强、闭环更完整，但引入复合系统的风险也更高，因此本任务书采用「**车顶臂 + 仅停稳后抓取**」的收敛方案，见 2.2 与 3.5。

---

## 2. 范围边界

### 2.1 In Scope

- AMR 本体新增机械臂 + 夹爪模型、关节控制器、TF 链。
- `GraspAction` / `PlaceAction`：替换 V2 BT 的 `Grasp` / `Place` 占位节点。
- 抓取成功/失败判定、失败重试与回退。
- 与 V3 货箱模型的 attach/detach 对接。
- 安全互锁：底盘锁止、`cmd_vel` 仲裁、急停双切、碰撞防护。
- 低电量抢占边界与机械臂耗电档位（V2 移交，见 3.6）：明确 BT `ReactiveFallback(BatteryOK?)` 能否打断臂动作，并按需启用 D 方案的 `MODE_ARM_ACTIVE` 扩展。
- 多机 namespace 下的模型、控制器、规划配置生成。

### 2.2 关键边界：只做「停稳后抓取」

```text
车到位 -> 制动并锁止底盘 -> 确认车速为 0 -> 机械臂动作 -> 货物 attach/detach
      -> 臂回安全位 -> 解除底盘锁止 -> 继续导航
```

**明确不做移动中抓取（mobile manipulation in motion）**。理由：底盘定位误差与臂末端误差叠加、需要整机协调控制、Gazebo 接触稳定性难以保证，属于另一个量级的问题；停稳后抓取可保留全部画面感与话题性，同时把风险收敛到可控范围。

### 2.3 Out of Scope（严格禁止）

| 不做 | 原因 / 归属 |
| --- | --- |
| 移动中抓取、底盘与臂协同控制、力控装配 | 本版明确排除 |
| 多臂协同、双臂操作 | 更后 |
| SLAM 重定位、路径规划算法研究、强化学习 | 岗位与版本均不涉及 |
| 视觉识别算法研究（若需相机仅用于校验，非算法） | TBD-030 |
| 真实硬件驱动、实机验证 | SRS 2.4 明确排除 |
| 改变 V1/V2 已冻结的契约（INV-01~12） | 冻结 |
| 修改 V2 行为树的**父树结构**、Guard、Fallback、任务级 FSM | 见 3.2，仅替换叶节点 |
| 改变 V3 货箱模型、触发时机与 `goods_status` 语义 | 见 3.4 |

### 2.4 一条质量红线

**如果最终只能做到「臂挥一下 → 货物瞬移 → 定时器推进状态」，则本版本不应宣告完成。** 那是把 SRS D-026 的「数据变化代替」升级成「动画代替」，面试官一句「你怎么判定抓取成功了」就会穿，比不做更糟。必须有可观测的成功/失败判定（TBD-031）。

---

## 3. 功能细节

### 3.1 机械臂模型与控制器

#### 3.1.1 V1 现状（本版从零开始的部分）

| 项 | 现状 | 证据 |
| --- | --- | --- |
| URDF/Xacro | 11 个文件全部是底盘 / 轮 / 脚轮 / 相机 / IMU / 激光，**无 arm、无 gripper** | `robot_ws/src/amr_description/urdf/` |
| ros2_control | 仅 `amr_diff_drive_controller` + `joint_state_broadcaster`，**无 `joint_trajectory_controller`** | `robot_ws/src/amr_description/config/amr_ros2_controller.yaml` |
| MoveIt | 无任何依赖或配置（SRDF / kinematics / OMPL） | 全仓检索无命中 |
| 货物模型 | V3 新增，V4 复用 | V3 任务书 |
| 相机 | `camera.urdf.xacro` 为普通 RGB box，非深度相机 | 文件内 `<box>` 几何 |

因此 V4 是**独立子项目级的工作量**，但与业务状态链路（MQTT / SQLite / 任务状态机）几乎零耦合，主要工作集中在 `amr_description` 与新增控制/规划包。

#### 3.1.2 模型要求

- 新增臂与夹爪的 xacro，遵循现有 `amr_v1.yaml` 几何参数单一来源的约定（V1 由 `spawn_amr.launch.py` 同时注入 xacro 与控制器 YAML）。
- **重调 mass / inertia**：车顶臂改变质心，差速底盘在 Gazebo 中可能不稳甚至翻车；必须实测并附调参记录（TBD-032）。
- 新增 `base_link -> arm_*` 的完整 TF 链，帧名遵循 **INV-11**：除 `map` 外使用 `robot_id + "_"` 前缀。
- 关节与夹爪需暴露 `ros2_control` 硬件接口，控制器配置与 `spawn_amr.launch.py` 的注入逻辑配套。
- 多机场景：每台 AMR 有独立 namespace 与独立 `controller_manager`（V1 已实现），臂控制器名称不得跨机冲突。

#### 3.1.3 控制方式（TBD-030）

| 方案 | 说明 | 取舍 |
| --- | --- | --- |
| A：预定义关节轨迹 | 用 `joint_trajectory_controller` 下发几个固定抓取姿态，不引入 MoveIt | 依赖少、配置简单、多机友好；但抓取姿态固定，泛化弱 |
| B：MoveIt2 | 完整 SRDF / kinematics / OMPL / planning scene | 能力完整、话题性最强；但配置量大，多机 namespace 下需按 `robot_id` 动态生成，且 Humble 版本匹配需验证 |

**建议先走 A 验证闭环，B 作为可选升级**，由 TBD-030 确认。无论哪种，对行为树都只暴露同一个 `GraspAction` 接口。

#### 3.1.4 多机配置生成（TBD-032）

若采用 MoveIt2，需参照 `robot_ws/src/amr_navigation/scripts/start_nav2.py` 的做法：按 `robot_id` 生成带前缀的配置副本，避免 N 台机器人共用一份配置导致 frame / topic 冲突。

---

### 3.2 行为树叶节点替换（本版本的核心约束）

#### 3.2.1 只允许改什么

```text
V2 树（不变）
  ReactiveFallback / Fallback / Sequence
  Guard(BatteryOK) / Guard(TrafficGranted) / Guard(GoodsApproved)
  Retry(Navigate) / AbortAndReport / Report
        |
        +-- Grasp    <-- V2: Wait(grasp_simulate_seconds) 占位
        +-- Place    <-- V2: Wait(place_simulate_seconds) 占位
                          V4: 替换为 GraspAction / PlaceAction（内部实现变更）
```

**父树结构、Guard、Fallback、Retry、任务级 FSM、对外 `task_state` 枚举、5 秒上报、`status/ack`、待补报、取消语义，全部不动。** 这是把行为树放在 V2 的全部意义所在；若 V4 需要改树结构，说明 V2 的抽象失败，必须先回头评审而不是直接改。

#### 3.2.2 `GraspAction` 接口要求

| 项 | 要求 |
| --- | --- |
| 输入 | `goods_id`、目标货箱实例、当前 `robot_id` / `instance_id`、抓取姿态来源 |
| 输出 | `SUCCESS` / `FAILURE`（BT 标准返回），供 `Retry` / `Fallback` 消费 |
| 成功判定 | 必须有可观测依据：接触检测 / 夹爪闭合反馈 / 货物 attach 成功回调 / 超时判定（TBD-031） |
| 失败处理 | 复用 V2 的 `Retry` 与 `AbortAndReport`；失败时**释放路权与配额**，上报 `CARRY_ABORTED`（沿用 V1 原因码或按契约新增） |
| 状态上报 | 仍复用 V2 的 `Report` 节点与 V1 消息结构，不新增 MQTT 字段 |
| 参数 | V1 的 `grasp_simulate_seconds` / `place_simulate_seconds` 在 V4 由真实动作替代；参数是否保留为超时上限由实现决定并写入配置说明 |

#### 3.2.3 与 V2 急停的联动

`GraspAction` 执行期间若急停触发，必须能中断臂动作并回到安全位；BT 侧表现为该节点返回 `FAILURE`，走既有 Fallback，不新增异常分支。

---

### 3.3 安全互锁（必须实现，不是可选项）

车顶臂属于协作机器人在移动平台上的应用，安全是面试官必问项。V2 已建立 `cmd_vel` 优先级仲裁，V4 必须复用并扩展：

| 要求 | 说明 |
| --- | --- |
| 臂运动前底盘锁止 | 通过 V2 的 `cmd_vel` mux 置零并加锁；确认车速为 0 后才允许臂动作 |
| 急停双切 | ESTOP 生效时同时切断底盘控制器与臂控制器，不可只停一边 |
| 显式复位 | 沿用 V2 急停的恢复语义，不自动恢复 |
| 碰撞防护 | 臂工作空间内不得与货架/其他 AMR 模型穿透；至少要有日志告警与中止 |
| 状态一致性 | 急停期间任务保持当前 `task_state`，不伪造完成、不丢失待补报（INV-03/09） |
| 失败回退 | 抓取失败后臂回安全位、释放资源，再进入 `AbortAndReport` |

> **为什么提前到 V2 建安全链路**：如果 V4 才补急停，会同时改动底盘控制与臂控制两条链路，风险叠加；V2 先建好、V4 只复用，是控制单次版本变量数的关键。

---

### 3.4 与 V3 货箱模型的对接

| V3 已有（保持不变） | V4 变化点 |
| --- | --- |
| 货箱 SDF / world、goods→模型映射配置 | 由 `GraspAction` / `PlaceAction` 触发挂载与卸载，而不是由状态推进直接触发 |
| attach/detach 能力 | 增加成功/失败回调给 BT 节点 |
| 触发时机（`GRASPING` / `PLACING`） | 不变 |
| 多机模型实例隔离 | 不变，臂侧需带同样的 `robot_id` / `instance_id` 维度 |
| `goods_status` 四量语义 | **不变**（INV-07） |

**不得**为了机械臂重新设计货箱模型或改动任务状态语义。

---

### 3.5 停稳后抓取的完整流程

```text
1. BT NavigateToPose 到达取货点，导航返回成功
2. 申请并确认底盘锁止（cmd_vel mux，车速 = 0）
3. GraspAction：
   3.1 读取 goods_id 与货箱实例位姿
   3.2 臂移动到预抓取位 -> 闭合夹爪 -> 接触/闭合反馈判定
   3.3 成功：触发 V3 attach，货物挂到臂/车体
   3.4 失败：按 TBD-031 判定，回安全位，交 Retry / Fallback
4. 臂回安全位，解除锁止
5. BT 继续 NavigateToPose 到放货点
6. PlaceAction 同理，触发 detach
7. Report 上报 CARRY_COMPLETED
```

---

### 3.6 低电量抢占策略与机械臂耗电（V2 移交 · TBD-037）

> **来源**：V2 分册 3.3.2.2 评审发现本分册原完全没有电量 / 抢占策略表述；随 TBD-036 冻结为 D 后，作为 **TBD-037** 移交本版处理。本小节只登记问题与边界，**方案待拍板，开工前必须明确**，不得直接实现。

**背景**：V2 的 BT 含反应式节点 `ReactiveFallback(BatteryOK?, GoCharge)`——`BatteryOK` 读取 `amr_power_manager` 发布的 `BatteryState.is_low`（V2 分册 3.3.2.3（5）），一旦低电量会**抢占**当前动作转向 `GoCharge`。V1/V2 只有底盘动作，抢占随时可打断；V4 的 `GraspAction` 执行期间底盘锁止、臂正在抓取货物，抢占可能导致「货物已 attach 但未上报」「臂停在半途」等不一致状态。

**待明确的三个问题**：

| # | 问题 | 评审建议（未冻结） |
| --- | --- | --- |
| 1 | 臂动作执行中电量跌破阈值，是否允许 `ReactiveFallback` 立即打断 `GraspAction`？ | 建议**不允许**——`GraspAction` 设为不可抢占叶节点（或在该节点内屏蔽 `BatteryOK` 抢占），待本节点返回 `SUCCESS`/`FAILURE`、资源释放后再回充；避免半途 attach |
| 2 | 与急停的边界如何区分？ | 急停是**安全事件**，必须能立即中断臂动作并回安全位（3.2.3 / 3.3）；低电量是**能量调度事件**，两者不可套用同一打断规则 |
| 3 | 是否体现机械臂执行器耗电？ | 可选。若启用，直接复用 D 方案扩展点：`PowerMode.msg` 加 `MODE_ARM_ACTIVE=3` + `amr_power_manager` 速率表加一行 `arm_drain_pct_per_min`，由 `GraspAction` 叶节点在 `tick()` 期间声明该档位；**不改 topic / 消息结构 / 积分逻辑 / 父树 / FSM / `amr_agent`**（V2 分册 3.3.2.3（7）） |

**必须遵守的红线**：

- **不改 V2 父树结构**、`Guard` / `Fallback` / `Retry`、任务级 FSM、`task_state` 枚举（2.3 / 3.2.1 已冻结）。若实现方案要求改父树，说明 V2 抽象失败，必须回头评审而不是就地打补丁。
- **不新建电量链路**：`BatteryOK` / `is_low` / SOC 真值全部复用 V2 的 `amr_power_manager` + `BatteryState`，本版只在 BT 侧定义抢占边界，不新增电量节点或 topic。
- B 方案是分钟级仿真简化模型，几秒级的臂动作耗电占比极小；问题 3 属**锦上添花**，问题 1 的抢占边界才是必须项——优先级不要颠倒。

---

## 4. 接口与数据模型变更汇总

| 类别 | 变更 | 位置 | 状态 |
| --- | --- | --- | --- |
| ROS2 模型 | 新增臂 + 夹爪 xacro、承载/末端 link、TF 链 | `robot_ws/src/amr_description/urdf/` | 新增 |
| ros2_control | 新增 `joint_trajectory_controller`（或等效）与硬件接口 | `amr_ros2_controller.yaml`、`spawn_amr.launch.py` | 新增 |
| MoveIt2 | SRDF / kinematics / OMPL 配置（若选方案 B） | 新增包或配置目录 | TBD-030 |
| BT 节点 | `Grasp` / `Place` 占位节点 → `GraspAction` / `PlaceAction` | `amr_behavior_manager` 的 BT XML 与 bt_nodes | **仅替换叶节点** |
| MQTT / HTTP / SQLite | **不变** | — | 冻结 |
| ROS2 消息 | 原则上不新增；若需臂状态反馈优先走内部 topic 且不进 `amr_common` 对外常量 | `amr_common/ros_interface_names.h` | 尽量不变 |
| 仿真 | 货箱 attach/detach 增加成功回调；mass/inertia 重调 | `simulation_ws/`、`amr_description/` | 新增 |

---

## 5. 文件级改动清单

| 类型 | 路径 | 说明 |
| --- | --- | --- |
| 新增 | `robot_ws/src/amr_description/urdf/actuator/arm.urdf.xacro`、`gripper.urdf.xacro`（建议名） | 臂与夹爪模型，含惯量宏 |
| 新增 | `robot_ws/src/amr_description/config/amr_arm_controller.yaml`（建议名） | 关节轨迹控制器参数 |
| 新增 | `robot_ws/src/amr_arm_control/` 或 `amr_manipulation/`（建议新包，TBD-030） | 抓取动作服务 / 轨迹下发 / 成功判定 |
| 新增 | `robot_ws/src/amr_behavior_manager/src/bt_nodes/grasp_action.cpp`、`place_action.cpp` | 替换占位节点，内部调上述包 |
| 新增 | MoveIt2 配置包（若方案 B） | SRDF、kinematics、OMPL、按 `robot_id` 生成脚本 |
| 新增 | `docs/architecture/adr/0004-onboard-manipulator.md`（建议） | 记录车顶臂 vs 工位臂的取舍、停稳后抓取边界、安全互锁 |
| 修改 | `robot_ws/src/amr_description/urdf/amr.urdf.xacro` | 组装臂与夹爪 |
| 修改 | `robot_ws/src/amr_description/config/amr_v1.yaml` | 几何参数与 mass/inertia（TBD-032） |
| 修改 | `robot_ws/src/amr_description/launch/spawn_amr.launch.py` | 注入臂控制器 YAML；确认多机 namespace 不冲突 |
| 修改 | `robot_ws/src/amr_behavior_manager/behavior_trees/*.xml` | **仅** `Grasp` / `Place` 节点实现替换 |
| 修改 | `robot_ws/src/amr_behavior_manager/package.xml`、`CMakeLists.txt` | 新增依赖需成对声明 |
| 修改 | V3 货箱 attach/detach 实现 | 增加成功/失败回调 |
| 修改 | `docs/requirements/srs.md` 2.4 / 8 章、`interface-contract.md`、`system.md`、`amr.md`、`amr-application-design.md` | **需项目所有者授权后修改** |
| 修改 | `README.md`、`docs/assets/architecture.svg` | 版本范围与架构图 |
| **禁止改动** | V2 BT 父树、任务级 FSM、`task_state` 枚举、MQTT 契约、SQLite 业务表、`amr_common` 现有常量语义 | 冻结 |

---

## 6. 文档同步清单

| 文档 | 动作 |
| --- | --- |
| `docs/requirements/srs.md` | 2.4 节「复杂的机械臂操作任务」由排除项改为 V4 范围；**需授权** |
| `docs/requirements/interface-contract.md` | 若新增臂状态反馈 topic/消息，需同步；否则注明「无对外接口变更」 |
| `docs/architecture/system.md` | 架构图新增执行层 |
| `docs/architecture/amr.md`、`amr-application-design.md` | 模型、控制器、抓取动作、安全互锁说明 |
| `docs/architecture/task-state-machine.md` | 重申「BT 内部动作 ≠ 对外 task_state」 |
| `docs/architecture/adr/README.md` | 新增 ADR-0004 索引 |
| `docs/development/build-run.md` | 新增臂相关启动/编译步骤与依赖 |
| `docs/development/nav2-tuning-notes.md` | 若 mass/inertia 影响导航，追加调参记录与快照 |
| `README.md` / `docs/assets/architecture.svg` | 更新 |
| `docs/architecture/version-roadmap.md` | 本分册状态更新（完成后） |

---

## 7. 实施顺序建议

1. **先关闭 TBD-030 / TBD-031 / TBD-032**，产出 ADR-0004，按重大变更流程征得同意（车顶臂属架构调整）。
2. **模型先行**：加 xacro 与惯量，先只做显示与 TF 验证，确认多机帧名不冲突、底盘不翻车。
3. **控制器**：接入 `joint_trajectory_controller`，手动下发几个固定姿态验证臂能到位。
4. **独立验证抓取**：暂不接 BT，用一个测试脚本跑「到位 → 闭合 → attach → detach」，确认成功/失败判定可靠（TBD-031）。
5. **接入 BT**：只替换 `Grasp` / `Place` 两个叶节点，逐条核对父树与对外 `task_state` 未变。
6. **安全互锁**：接 V2 急停，验证臂运动期间底盘锁止、ESTOP 双切。
7. **失败路径**：注入抓取失败，验证 Retry / Fallback / 资源释放 / `CARRY_ABORTED` 上报。
8. **验证与 DoD**：录制演示、文档同步、不变式复核。

---

## 8. 验收清单

- [ ] 车顶臂与夹爪模型在 Gazebo / RViz 中正确显示，多机 TF 帧名遵循 `robot_id + "_"` 前缀且不冲突。
- [ ] 加装臂后底盘不翻车、导航稳定性未退化；mass/inertia 调整有调参记录与快照。
- [ ] `joint_trajectory_controller` 可稳定到达预抓取/放置姿态。
- [ ] 抓取成功/失败有**可观测依据**（非固定延时模拟），失败能被正确判定并触发 Retry / Fallback。
- [ ] V2 行为树**父树结构、Guard、Fallback、Retry 完全未改**；对外 `task_state` 枚举与 V1 一致。
- [ ] V3 货箱模型、触发时机、`goods_status` 四量语义未变；attach/detach 由 GraspAction 触发。
- [ ] 臂运动前底盘锁止且车速为 0；ESTOP 同时切断底盘与臂控制器；显式复位后恢复。
- [ ] TBD-037 已拍板并有实测：`GraspAction` 执行中低电量时按结论处理（建议不打断、在本节点返回后再回充），不出现「货物半 attach / 臂停半途」，且父树结构与 `task_state` 未变。
- [ ] 抓取失败后臂回安全位、释放路权与配额、上报 `CARRY_ABORTED`。
- [ ] 多机场景下每台 AMR 的臂控制器与规划配置互不干扰。
- [ ] 第 3 节全部跨版本不变式逐条复核未破坏。
- [ ] `colcon build` 通过；`package.xml` 与 `CMakeLists.txt` 依赖一致；新节点已注册 install 目标。
- [ ] 第 6 节文档同步完成；README 与架构图更新。
- [ ] 提交说明附「为什么这么实现」（车顶臂 vs 工位臂取舍、停稳后抓取边界、叶节点替换而非树重构、安全互锁复用 V2）。

---

## 9. 本版本 TBD

| 编号 | 问题 | 需要明确的具体内容 | 开工前必须明确 |
| --- | --- | --- | --- |
| TBD-030 | 机械臂形态与控制 | 自由度、夹爪类型、方案 A（预定义轨迹）还是 B（MoveIt2）、是否引入深度相机 | 是 |
| TBD-031 | 抓取成功判定 | 接触检测 / 夹爪闭合反馈 / attach 回调 / 超时阈值；失败重试次数与回退策略 | 是 |
| TBD-032 | 车顶臂对模型与 TF 的影响 | mass/inertia 重调范围、`base->arm` TF 链、多机 namespace 下规划配置生成方式 | 是 |
| TBD-033 | 演示与证据归档（引用） | 抓取成功/失败日志、Groot 片段、录像存放路径 | 否 |
| TBD-037 | 低电量抢占策略（V2 移交） | `ReactiveFallback(BatteryOK?)` 能否打断 `GraspAction`；与急停打断的边界；是否启用 `MODE_ARM_ACTIVE` 臂耗电档位。见 3.6；**不得改 V2 父树 / FSM / `task_state`** | 是 |

---

## 10. 为什么这么实现（取舍）

- **车顶臂而非工位侧固定臂**：项目所有者倾向移动操作，它在演示画面、话题性（具身智能 / 移动操作）和闭环完整度上更强。代价是底盘误差与臂末端误差叠加、复合控制难度、多机配置复杂度——因此用「仅停稳后抓取」把风险收敛。
- **先模型、再控制器、再独立验证抓取、最后接 BT**：把四个新变量拆开逐个验证，避免一次性集成时无法定位问题。
- **只替换叶节点**：V2 把行为树提前落地的全部收益就在这一步兑现。若 V4 需要改父树，说明 V2 抽象有问题，应回头评审而不是就地打补丁。
- **安全互锁复用 V2**：急停与 `cmd_vel` 仲裁在 V2 已经建好，V4 只扩展「双切」与「锁止」，不新起一套安全机制。
- **成功判定必须可观测**：这是「真实闭环」和「动画」的分界线。做不到就不要宣告完成——面试官区分 demo 与工程，主要看异常路径与判定依据，而抓取恰恰是全项目最容易被追问「怎么判定成功」的环节。
- **不改 MQTT / SQLite / 任务状态机**：机械臂是执行层内部能力，不应污染已验证的跨进程契约，这样 V1~V3 的验收场景在 V4 之后仍可回归。

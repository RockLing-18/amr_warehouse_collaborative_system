# Nav2 调参经验与稳定性笔记

**状态：** Implemented
**日期：** 2026-11-17
**范围：** `amr_navigation` / `amr_bringup` / `amr_behavior_manager`

## 1. 背景

V1 验收前，AMR 在 Gazebo 仿真中出现三类导航问题：

1. 到点后反复 `Failed to make progress`，机器人看起来“原地犹豫”。
2. 个别站点（如 B 货物存储区）放货后回充电点卡住，路径已生成但无法起步。
3. 日志刷屏、启动参数类型错误等工程问题。

本文记录定位过程、根因和最终参数调整，作为后续维护与面试讲解素材。

## 2. 参数快照

当前生效配置：

- `docs/development/nav2-params-snapshots/nav2_params_2026-11-17-reeds-shepp-reverse.yaml`

历史对照：

- `docs/development/nav2-params-snapshots/nav2_params_2026-11-12.yaml`
- `docs/development/nav2-params-snapshots/nav2_params_2026-11-16-smac-good.yaml`

## 3. 问题、根因与处理

### 3.1 到点后“犹豫”并反复失败

**现象**

到达取货/放货点附近后，`controller_server` 每隔约 10 秒打印一次 `Failed to make progress`，随后清空 costmap、重新规划，表现像原地犹豫。

**根因**

`SimpleProgressChecker` 的判定是“在 `movement_time_allowance` 时间内平移达到 `required_movement_radius`”。它只统计平移距离，**不把原地转向算作进展**。

AMR 到达站点后，下一段路径往往需要先原地掉头或大角度转向；转向还没完成就被误判为卡住，于是反复中止。

**处理**

```yaml
progress_checker:
  plugin: "nav2_controller::SimpleProgressChecker"
  required_movement_radius: 0.5
  movement_time_allowance: 20.0   # 原 10.0
```

同时加快原地转向：

```yaml
FollowPath:
  acc_lim_theta: 0.8    # 原 0.2
  decel_lim_theta: -0.8 # 原 -0.5
```

**经验**

“卡住”要先区分是“真的走不动”还是“进度判定太苛刻”。旋转不产生位移，不能用纯平移距离来衡量进度。

### 3.2 目标容差的取舍

`xy_goal_tolerance` / `yaw_goal_tolerance` 是“到达判定”阈值，不是驱动能力。

- 放宽容差：更容易判定到达，导航容错更高。
- 放宽的代价：停点精度下降，可能偏 0.15 m / 0.2 rad。

当前采用粗导航档位：

```yaml
general_goal_checker:
  xy_goal_tolerance: 0.15
  yaw_goal_tolerance: 0.2
FollowPath:
  xy_goal_tolerance: 0.15
```

**经验**

稳定性和精度要分层：干线导航用宽松容差保证“必达”，末端对接再降速并把容差收紧到 `0.01~0.02 m`。

### 3.3 站点旁卡住、无法后退

**现象**

B 货物存储区放货后回充电点，路径已生成但机器人不动，最终 `no valid path found` / `Failed to make progress`。

**根因**

静态地图显示该站点附近有竖直货架墙和上方横墙，机器人停在一个“右侧有墙、上方有墙”的凹角。原配置：

```yaml
FollowPath:
  min_vel_x: 0.0
GridBased:
  motion_model_for_search: "DUBIN"
```

DWB 不允许倒车，DUBIN 又只能规划前进路径，因此机器人没有空间完成前向转弯，陷入死局。

**处理**

```yaml
FollowPath:
  min_vel_x: -0.26   # 允许 DWB 倒车
GridBased:
  motion_model_for_search: "REEDS_SHEPP"  # 允许带倒车的全局路径
  reverse_penalty: 2.0
```

`velocity_smoother.min_velocity.x` 本来就允许 `-0.26`，无需额外改动。

**经验**

差速底盘遇到狭小空间时，倒车是有效的脱困手段。局部控制器、全局规划器、速度平滑器三者对“倒车能力”的约束要一致。

### 3.4 日志刷屏与启动参数类型错误

**日志刷屏**

`navigation_server` 的 `current(...) remain ...` 由每次反馈打印改为 5 秒一条，降低日志噪音，便于定位真正异常。

**启动参数类型错误**

`use_sim_time` 等参数在 launch 文件中需显式转成布尔再传给节点，避免 `InvalidParameterTypeException`。

## 4. 仍待观察的风险

日志中偶尔出现：

```text
Message Filter dropping message ... earlier than all the data in the transform cache
Transform data too old when converting from robot01_odom to map
```

这提示 `/clock`、激光数据、TF 时间戳可能不完全同步。当前通过放宽时间容差规避，后续如果继续出现定位跳变或规划失败，应优先排查时间同步，而不是继续调参。

## 5. 面试陈述提纲

1. **先说目标**：让 AMR 在仿真仓库里稳定完成多趟、多货物搬运任务。
2. **再说方法论**：先看日志找根因，不盲目改参数；每个改动都记录现象、根因、参数、可回退快照。
3. **讲三个典型案例**：
   - 进度检查器不统计旋转，导致到点误判卡住；
   - 目标容差是到达判定，容错与精度需要分层；
   - 差速机器人不会倒车，在凹角站点无法脱困。
4. **总结取舍**：导航稳定性 = 正确的时间/TF 基础 + 合理的到达判定 + 与底盘能力匹配的运动模型。
5. **强调工程习惯**：小步修改、保存快照、用日志证据验证，而不是“调一次试一次”。


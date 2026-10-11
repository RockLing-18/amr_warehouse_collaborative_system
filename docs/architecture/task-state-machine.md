# 任务状态机与 AMR 行为状态机

**Status:** Draft

本文档细化任务级状态机和 AMR 行为状态机。状态枚举来自已确认的 ADR 和接口契约；状态之间的触发条件和动作规则中，标注“已确认”的部分属于待确认设计，落地实现前需要复核。

## 1. 三套状态的区别

系统中有三套状态，不能混用：

| 维度 | 状态来源 | 含义 | 维护方 |
| --- | --- | --- | --- |
| 任务级 `state` | ADR-0001 决策 18 | 一个运输任务的整体生命周期 | Edge Server |
| AMR 行为 `task_state` | 接口契约 4.6 | 单个 AMR 执行任务时的行为阶段 | AMR，Edge 记录快照 |
| 机器人运行 `RobotState` | `edge_server/src/types.h` | 机器人接入和运行状态 | Edge Server |

`RobotState.IDLE` 与 AMR `task_state.IDLE` 同名但含义不同：前者表示机器人整体空闲，后者表示当前任务行为为空闲。

## 2. 任务级状态机

### 2.1 状态集合

```text
PENDING -> ACCEPTED -> RUNNING -> SUCCEEDED
   |           |          |---------> FAILED
   |           |          |---------> INTERRUPTED
   |           |--------> CANCELLED
   |------------------> CANCELLED
```

### 2.2 状态说明

| 状态 | 进入条件 | 说明 |
| --- | --- | --- |
| `PENDING` | 通过 HTTP 任务注入创建成功 | 任务尚未被任何 AMR 接受。 |
| `ACCEPTED` | 至少一个参与 AMR 上报接受 | 任务已下发并至少有一个执行者。 |
| `RUNNING` | 第一个货物申请获批 | 至少有一个 AMR 开始搬运。 |
| `SUCCEEDED` | 所有货物 `pending_quantity=0` 且 `in_progress_quantity=0` | 全部货物完成搬运。 |
| `FAILED` | 任务不可恢复失败 | 预留状态，V1 主要使用取消和中断。 |
| `CANCELLED` | 取消流程完成 | 任务被显式取消。 |
| `INTERRUPTED` | 参与任务的全部 AMR 异常 | 任务级中断，可后续恢复判定。 |

### 2.3 状态转换表

| 当前状态 | 事件 | 下一状态 | 动作 | 状态 |
| --- | --- | --- | --- | --- |
| `PENDING` | 任务下发并收到首个 AMR 接受 | `ACCEPTED` | 更新 `tbl_task.state` | 已确认 |
| `ACCEPTED` | 首个货物申请获批 | `RUNNING` | 更新 `tbl_task_goods_allocation` 和 `tbl_task_goods_status` | 已确认 |
| `RUNNING` | 全部货物完成 | `SUCCEEDED` | 发布终态共享状态 | 已确认终态集合 |
| `PENDING` / `ACCEPTED` / `RUNNING` | 取消请求成功 | `CANCELLED` | 通知参与 AMR，发布终态 | 已确认 |
| `RUNNING` | 执行发生不可恢复错误 | `FAILED` | 发布终态 | 已确认 |
| `RUNNING` | 全部参与 AMR 异常 | `INTERRUPTED` | 发布 `INTERRUPTED` | 已确认规则 |

### 2.4 状态覆盖规则

- 终态 `SUCCEEDED`、`FAILED`、`CANCELLED`、`INTERRUPTED` 不应被旧消息回退。
- 状态更新按 `task_id + state + timestamp` 判定，旧消息不覆盖新状态。
- 取消请求到达时如果任务已进入终态，返回已有终态，不覆盖。

## 3. AMR 行为状态机

### 3.1 状态集合

```text
IDLE -> ACCEPTED -> GRASPING -> TRANSPORTING -> PLACING -> IDLE
         |              |            |              |
         +--------------+------------+--------------+--> INTERRUPTED
```

### 3.2 状态说明

| 状态 | 含义 |
| --- | --- |
| `IDLE` | 无任务或等待下一步。 |
| `ACCEPTED` | 已接受任务，等待/申请货物。 |
| `GRASPING` | 正在抓取货物。 |
| `TRANSPORTING` | 正在搬运货物。 |
| `PLACING` | 正在放置货物。 |
| `INTERRUPTED` | 因电量低、导航失败等原因中断。 |

### 3.3 状态转换表

| 当前状态 | 事件 | 下一状态 | 动作 | 状态 |
| --- | --- | --- | --- | --- |
| `IDLE` | 收到任务并接受 | `ACCEPTED` | 上报接受 | 已确认 |
| `ACCEPTED` | 货物申请获批并到达装货点 | `GRASPING` | 开始抓取 | 已确认 |
| `GRASPING` | 抓取完成 | `TRANSPORTING` | 更新当前搬运货物 | 已确认 |
| `TRANSPORTING` | 到达卸货点 | `PLACING` | 开始放置 | 已确认 |
| `PLACING` | 放置完成且仍有待搬运 | `GRASPING` | 继续下一趟 | 已确认 |
| `PLACING` | 放置完成且无待搬运 | `IDLE` | 结束任务 | 已确认 |
| `ACCEPTED` / `GRASPING` / `TRANSPORTING` / `PLACING` | 电量低、导航失败、手动停止 | `INTERRUPTED` | 上报放弃/中断 | 已确认 |
| `INTERRUPTED` | 收到恢复/继续指令 | `IDLE` | 等待新任务或新指令 | 已确认 |

### 3.4 与货物申请的边界

- AMR 在 `ACCEPTED` 状态下申请下一趟货物，只有 `amr/{robot_id}/task/goods/response` 成功后才从 `ACCEPTED` 进入 `GRASPING`。
- 正在搬运的货物数量对应 `tbl_task_goods_allocation` 中的 `IN_PROGRESS` 分配。
- Edge 不可用时，AMR 完成当前已获批搬运后不再发起新申请，并把结果写入 `pending_task_reports.json`。

## 4. 事件与动作

### 4.1 任务级事件来源

| 事件 | 来源 | 说明 |
| --- | --- | --- |
| 任务创建 | HTTP API | 进入 `PENDING`。 |
| 任务接受 | AMR `STATUS` | 首个 AMR 接受后进入 `ACCEPTED`。 |
| 申请获批 | 货物申请响应 | 首个获批后进入 `RUNNING`。 |
| 完成上报 | AMR `CARRY_COMPLETED` | 更新货物进度，可能进入 `SUCCEEDED`。 |
| 放弃上报 | AMR `CARRY_ABORTED` | 回退在途数量。 |
| 取消请求 | HTTP API | 进入 `CANCELLED`。 |
| 心跳超时 | Edge 内部计时 | 判定 AMR 异常。 |

### 4.2 超时与异常

- AMR 任务状态每 5 秒上报一次。
- Edge 5 分钟未收到 AMR 状态更新视为该 AMR 异常。
- 单个 AMR 异常但仍有其他可用 AMR 时，任务继续，不触发任务级 `INTERRUPTED`。
- 全部参与 AMR 异常时，Edge 上报任务级 `INTERRUPTED`，`reason=ALL_AMRS_ABNORMAL`。

## 5. 与数据模型的映射

| 状态机对象 | 数据表 | 字段 |
| --- | --- | --- |
| 任务级状态 | `tbl_task` | `state` |
| 任务级状态变化 | `tbl_task_event` | `state`、`source`、`timestamp` |
| AMR 行为状态 | `tbl_amr_task_status_event` | `task_state` |
| 货物进度 | `tbl_task_goods_status` | 四个数量字段 |
| 在途分配 | `tbl_task_goods_allocation` | `state`、`quantity` |

## 6. 已确认事项

- `ACCEPTED` 已确认：Edge 已成功下发任务，且至少一个参与 AMR 上报 `ACCEPTED`。
- `INTERRUPTED` 已确认：不自动继续后续任务，需人工或上层重新创建任务。
- `FAILED` 已确认：V1 不主动使用，后续版本保留。
- AMR 从 `INTERRUPTED` 已确认恢复到 `IDLE`。

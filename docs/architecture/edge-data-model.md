# Edge 数据模型与 SQLite Schema

**Status:** Draft

本文档定义 Edge Server 的 SQLite 数据模型，用于支撑 SRS 中的任务闭环、机器人管理、地图管理和 Edge 故障恢复后的待补报合并。文档描述的是目标 Schema，尚未在代码中落地，状态标记为 `Planned`。

## 1. 范围与权威关系

本数据模型需要满足：

- `docs/requirements/srs.md` 第 6.6 节的“关键数据对象”；
- `docs/requirements/interface-contract.md` 第 4 节定义的 MQTT 消息字段和幂等规则；
- `docs/architecture/adr/0001-edge-task-injection-and-mqtt-task-protocol.md` 的 28、29、30 号决策。

本文档不改变已有 `tbl_map_package` 和 `tbl_active_map` 两张地图表，只新增任务、机器人、仿真实例、状态事件相关的表。

## 2. 状态枚举

### 2.1 任务级状态

来源为 ADR-0001 决策 18，完整转换规则见 `docs/architecture/task-state-machine.md`。

| 值 | 说明 |
| --- | --- |
| `PENDING` | 任务已创建，尚未被 AMR 接受。 |
| `ACCEPTED` | 任务已下发，至少一个参与 AMR 已接受。 |
| `RUNNING` | 任务正在搬运。 |
| `SUCCEEDED` | 任务全部货物完成搬运。 |
| `FAILED` | 任务不可恢复地失败。 |
| `CANCELLED` | 任务被取消。 |
| `INTERRUPTED` | 参与任务的全部 AMR 异常，任务级中断。 |

### 2.2 AMR 任务状态（`task_state`）

来源为接口契约 4.6 节，表示单个 AMR 在执行任务时的行为状态。

| 值 | 说明 |
| --- | --- |
| `IDLE` | 无任务或等待下一步。 |
| `ACCEPTED` | 已接受任务。 |
| `GRASPING` | 正在抓取货物。 |
| `TRANSPORTING` | 正在搬运货物。 |
| `PLACING` | 正在放置货物。 |
| `INTERRUPTED` | 因电量低、导航失败等原因中断。 |

### 2.3 机器人运行状态

沿用 `edge_server/src/types.h` 中 `RobotState` 的枚举，表示机器人接入和运行层面的状态，与 AMR 任务状态是两套维度。

| 值 | 说明 |
| --- | --- |
| `OFFLINE` | 离线。 |
| `INITIALIZING` | 初始化中，不可接任务。 |
| `IDLE` | 空闲，可接任务。 |
| `WORKING` | 执行任务中。 |
| `CHARGING` | 充电。 |
| `PAUSED` | 暂停。 |
| `ERROR` | 故障。 |

## 3. 表总览

| 表名 | 用途 | 对应 SRS 6.6 数据对象 | 状态 |
| --- | --- | --- | --- |
| `tbl_robot` | 机器人注册信息 | 机器人注册信息 | Planned |
| `tbl_robot_instance` | 仿真实例与业务身份映射 | 仿真 AMR 实例 | Planned |
| `tbl_robot_status` | 机器人最新运行状态快照 | 机器人运行状态 | Planned |
| `tbl_task` | 运输任务主记录 | 运输任务 | Planned |
| `tbl_task_goods` | 任务中的货物定义 | 运输任务（货物明细） | Planned |
| `tbl_task_goods_status` | 每类货物的搬运进度 | 运输任务（进度） | Planned |
| `tbl_task_goods_allocation` | 各 AMR 正在搬运的货物分配 | Edge 内部状态 | Planned |
| `tbl_task_event` | 任务级状态变化审计 | 任务状态事件 | Planned |
| `tbl_amr_task_status_event` | AMR 原始状态/完成/放弃事件 | 任务状态事件（AMR 来源） | Planned |

`tbl_map_package`、`tbl_active_map` 为已实现的地图表，不在本表重复定义。

## 4. 表结构

时间字段统一使用 ISO 8601 文本，与 MQTT/HTTP 消息中的 `timestamp` 保持一致。状态和枚举字段均使用 `TEXT` 存储，便于直接与 JSON 消息对齐；约束由 `CHECK` 和服务层共同保证。

### 4.1 tbl_robot

机器人业务身份，一条记录对应一个 `robot_id`。

```sql
CREATE TABLE IF NOT EXISTS tbl_robot
(
    robot_id            TEXT PRIMARY KEY,
    warehouse_id        TEXT NOT NULL,
    robot_name          TEXT,
    register_timestamp  TEXT NOT NULL,
    updated_at          TEXT NOT NULL,
    initial_pose        TEXT
);
```

字段说明：

- `robot_id`：机器人业务身份，主键。
- `warehouse_id`：所属仓库。
- `register_timestamp`：首次注册时间。
- `robot_name`：显示名，可选。
- `initial_pose`：注册时分配的初始点位 JSON，例如 `{"x":1.0,"y":2.0,"yaw":0.5}`，与 `tbl_robot_status.pose`（实时位置）分离。

### 4.2 tbl_robot_instance

仿真 AMR 实例与业务身份之间的映射。`robot_id` 与 `instance_id` 联合唯一。

```sql
CREATE TABLE IF NOT EXISTS tbl_robot_instance
(
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    robot_id         TEXT NOT NULL,
    instance_id      TEXT NOT NULL,
    model_name       TEXT,
    lifecycle_state  TEXT NOT NULL,
    created_at       TEXT NOT NULL,
    updated_at       TEXT NOT NULL,
    UNIQUE(robot_id, instance_id)
);

CREATE INDEX IF NOT EXISTS idx_robot_instance_robot
    ON tbl_robot_instance(robot_id);
```

### 4.3 tbl_robot_status

机器人最新运行状态，一个 `robot_id` 只保留一条最新快照。

```sql
CREATE TABLE IF NOT EXISTS tbl_robot_status
(
    robot_id        TEXT PRIMARY KEY,
    state           TEXT NOT NULL,
    online          INTEGER NOT NULL DEFAULT 0,
    battery         REAL,
    pose            TEXT,
    current_task_id TEXT,
    updated_at      TEXT NOT NULL
);
```

字段说明：

- `state`：`RobotState` 枚举。
- `online`：`1` 表示在线，`0` 表示离线。
- `pose`：JSON 文本，例如 `{"x":1.0,"y":2.0,"theta":0.5}`。
- `current_task_id`：当前任务，无任务时可为空。

### 4.4 tbl_task

运输任务主记录。`robot_ids` 以 JSON 数组文本保存，保证与接口契约消息字段一致。

```sql
CREATE TABLE IF NOT EXISTS tbl_task
(
    task_id      TEXT PRIMARY KEY,
    warehouse_id TEXT NOT NULL,
    task_type    TEXT NOT NULL CHECK(task_type IN ('LOAD', 'UNLOAD')),
    state        TEXT NOT NULL,
    robot_ids    TEXT NOT NULL,
    goal         TEXT,
    reason       TEXT,
    created_at   TEXT NOT NULL,
    updated_at   TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_task_warehouse_state
    ON tbl_task(warehouse_id, state);
```

字段说明：

- `task_type`：`LOAD` 或 `UNLOAD`。
- `state`：任务级状态枚举。
- `robot_ids`：参与 AMR 的 JSON 数组，例如 `["robot_01","robot_02"]`。
- `goal`：可选，V1 目的地由 `goods_id + task_type` 映射，暂不需要存储坐标，保留该字段以对应 SRS 关键数据对象。
- `reason`：失败、取消、中断等原因。

### 4.5 tbl_task_goods

任务中的静态货物定义，一次任务可以包含多类货物。

```sql
CREATE TABLE IF NOT EXISTS tbl_task_goods
(
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    task_id        TEXT NOT NULL,
    goods_id       TEXT NOT NULL,
    goods_name     TEXT,
    total_quantity INTEGER NOT NULL CHECK(total_quantity >= 0),
    UNIQUE(task_id, goods_id)
);

CREATE INDEX IF NOT EXISTS idx_task_goods_task
    ON tbl_task_goods(task_id);
```

### 4.6 tbl_task_goods_status

每类货物的搬运进度。为了在数据库层保持一致性，`total_quantity` 在此表冗余一份，并加入等式约束。

```sql
CREATE TABLE IF NOT EXISTS tbl_task_goods_status
(
    id                   INTEGER PRIMARY KEY AUTOINCREMENT,
    task_id              TEXT NOT NULL,
    goods_id             TEXT NOT NULL,
    total_quantity       INTEGER NOT NULL CHECK(total_quantity >= 0),
    in_progress_quantity INTEGER NOT NULL DEFAULT 0 CHECK(in_progress_quantity >= 0),
    completed_quantity   INTEGER NOT NULL DEFAULT 0 CHECK(completed_quantity >= 0),
    pending_quantity     INTEGER NOT NULL DEFAULT 0 CHECK(pending_quantity >= 0),
    updated_at           TEXT NOT NULL,
    UNIQUE(task_id, goods_id),
    CHECK(total_quantity = in_progress_quantity + completed_quantity + pending_quantity)
);

CREATE INDEX IF NOT EXISTS idx_task_goods_status_task
    ON tbl_task_goods_status(task_id);
```

一致性规则：

- 对每一类货物：`in_progress_quantity + completed_quantity + pending_quantity = total_quantity`。
- `total_quantity` 与 `tbl_task_goods.total_quantity` 保持一致，由 Task Service 写入。

### 4.7 tbl_task_goods_allocation

记录每个 AMR 已获批但尚未结算的货物数量。它同时承担两件事：

1. 货物申请幂等：`request_id + robot_id` 唯一，避免同一申请被重复批准。
2. Edge 内部维护“正在搬运该货物的 AMR 列表”，该信息不进入任务共享状态，也不下发其他 AMR。

```sql
CREATE TABLE IF NOT EXISTS tbl_task_goods_allocation
(
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    request_id    TEXT NOT NULL,
    task_id       TEXT NOT NULL,
    goods_id      TEXT NOT NULL,
    robot_id      TEXT NOT NULL,
    quantity      INTEGER NOT NULL CHECK(quantity > 0),
    state         TEXT NOT NULL DEFAULT 'IN_PROGRESS'
                  CHECK(state IN ('IN_PROGRESS', 'COMPLETED', 'ABORTED')),
    allocated_at  TEXT NOT NULL,
    settled_at    TEXT,
    UNIQUE(request_id, robot_id)
);

CREATE INDEX IF NOT EXISTS idx_task_goods_allocation_active
    ON tbl_task_goods_allocation(task_id, goods_id, state);

CREATE INDEX IF NOT EXISTS idx_task_goods_allocation_robot
    ON tbl_task_goods_allocation(robot_id);
```

字段说明：

- `request_id`：AMR 货物申请的唯一标识。
- `quantity`：该申请获批的搬运数量。
- `state`：`IN_PROGRESS` 表示正在搬运；`COMPLETED` 表示已完成；`ABORTED` 表示已放弃/中断回退。

结算规则：

- `CARRY_COMPLETED`：将该分配置为 `COMPLETED`，同时 `tbl_task_goods_status.in_progress_quantity` 减少、`completed_quantity` 增加。
- `CARRY_ABORTED`：将该分配置为 `ABORTED`，同时 `in_progress_quantity` 减少、`pending_quantity` 增加。

### 4.8 tbl_task_event

任务级状态变化审计表，用于补报、审计和冲突诊断。

```sql
CREATE TABLE IF NOT EXISTS tbl_task_event
(
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    task_id    TEXT NOT NULL,
    state      TEXT NOT NULL,
    reason     TEXT,
    source     TEXT NOT NULL,
    timestamp  TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_task_event_task_time
    ON tbl_task_event(task_id, timestamp);
```

`source` 建议值：

- `HTTP`：任务注入或取消 API。
- `AMR`：AMR 状态上报。
- `EDGE`：Edge 内部规则判定。
- `EDGE_RECOVERY`：Edge 故障恢复后的合并。
- `TIMEOUT`：心跳超时或任务超时。

### 4.9 tbl_amr_task_status_event

保存 AMR 原始状态/完成/放弃事件，是待补报合并和幂等去重的核心表。

```sql
CREATE TABLE IF NOT EXISTS tbl_amr_task_status_event
(
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    message_id   TEXT NOT NULL,
    task_id      TEXT NOT NULL,
    robot_id     TEXT NOT NULL,
    event_type   TEXT NOT NULL
                 CHECK(event_type IN ('STATUS', 'CARRY_COMPLETED', 'CARRY_ABORTED')),
    task_state   TEXT NOT NULL,
    goods_json   TEXT,
    reason       TEXT,
    timestamp    TEXT NOT NULL,
    received_at  TEXT NOT NULL,
    UNIQUE(message_id, task_id, robot_id, event_type)
);

CREATE INDEX IF NOT EXISTS idx_amr_task_status_event_task_time
    ON tbl_amr_task_status_event(task_id, timestamp);
```

字段说明：

- `message_id + task_id + robot_id + event_type`：接口契约 4.9 节规定的幂等去重键。
- `goods_json`：`goods` 数组的 JSON 原文。
- `timestamp`：AMR 上报时间，用于避免旧消息覆盖新状态。
- `received_at`：Edge 实际接收时间，用于诊断乱序和延迟。

## 5. goods_status 的派生

任务共享状态消息中的 `goods_status` 不是单独一张表，而是由以下数据派生：

- `tbl_task_goods` 提供 `goods_id`、`goods_name`、`total_quantity`；
- `tbl_task_goods_status` 提供 `in_progress_quantity`、`completed_quantity`、`pending_quantity`；
- `tbl_task_goods_allocation` 维护 Edge 内部“哪些 AMR 正在搬运”，用于批准、完成和回退计算。

发布 `warehouse/{warehouse_id}/task/{task_id}/state` 时，按任务中所有货物的顺序生成 `goods_status`，顶层四个数量字段分别是所有货物对应字段之和。

## 6. 幂等键汇总

| 场景 | 幂等键 | 说明 |
| --- | --- | --- |
| 任务创建 | `task_id` | 重复注入不重复创建任务。 |
| 任务下发 | `task_id + message_id` | 避免重复下发导致重复接受。 |
| 货物申请 | `request_id + robot_id` | 避免重复批准同一申请。 |
| AMR 状态/完成/放弃 | `message_id + task_id + robot_id + event_type` | 待补报重传去重。 |
| 任务状态覆盖 | `task_id + state + timestamp` | 终态不被旧消息回退。 |

## 7. Edge 恢复与待补报合并

Edge 重启或 MQTT 恢复后的合并顺序：

1. 从 `tbl_task`、`tbl_task_goods`、`tbl_task_goods_status`、`tbl_task_goods_allocation` 恢复任务和进度。
2. 从 `tbl_amr_task_status_event` 恢复历史 AMR 事件。
3. 接收 AMR 重传的 `pending_task_reports.json` 事件。
4. 用 `message_id + task_id + robot_id + event_type` 去重。
5. 用 `timestamp` 判定新旧，拒绝旧消息覆盖新状态。
6. 合并完成后发布最新任务共享状态。

## 8. 与既有表的关系

- `tbl_map_package` 和 `tbl_active_map` 保持不变，继续由 Map Service 使用。
- 本 Schema 中机器人、任务表不迁移地图数据，也不改变现有地图接口。

## 9. 已确认事项

- `tbl_task.robot_ids` 已确认：V1 使用 JSON 数组存储，不新增 `tbl_task_robot` 成员表。
- `tbl_task.goal` 已确认：V1 保留字段但允许为空。
- AMR 本地待补报记录已确认：收到 Edge `amr/{robot_id}/task/status/ack` 后删除对应记录。
- `tbl_task_event` 与 `tbl_amr_task_status_event` 已确认：V1 保留两张表，不合并。

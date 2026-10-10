# V1 接口契约

## 1. 目的与状态

本文档记录 V1 已确定或待冻结的接口约束，是后续 Edge、AMR、Simulation 和任务相关代码开发的接口依据。

当前状态：Draft。

已确定：

- V1 使用 Edge 侧 HTTP API 进行任务注入。
- 后续 Cloud 使用 MQTT 向 Edge 发布任务，Edge 订阅任务 Topic。
- 任务相关 MQTT Topic 使用 `warehouse/{warehouse_id}/task`、`warehouse/{warehouse_id}/task/status`、`amr/{robot_id}/task`、`warehouse/{warehouse_id}/task/{task_id}/state`、`amr/{robot_id}/task/goods/request`、`amr/{robot_id}/task/goods/response` 和 `amr/{robot_id}/task/status`。
- AMR 内部 ROS2 Topic/Service/Action 名称统一定义在 `amr_common`。
- Edge/Cloud/AMR 相关 MQTT Topic 统一定义在 `common/mqtt/mqtt_topic.h`。

待确认：

- Edge 故障恢复策略：AMR 完成当前已批准搬运后停止下一行动，待 Edge 恢复后重传本地待补报结果；该方案仍待最终确认。
- 任务消息是否需要消息级确认 ACK，还是仅依赖 QoS 1 + 幂等字段。
- 任务状态枚举是否需要与未来 Cloud 状态机完全一致。

已确认：

- Edge 任务注入 HTTP API 无鉴权，允许局域网内调用，统一使用 `code + msg + data` 响应结构。
- AMR 任务状态每 5 秒上报一次；Edge 5 分钟未收到更新即视为异常。

## 2. 命名约定

- JSON 字段使用 `snake_case`。
- MQTT Topic 使用小写和 `/` 分层。
- 枚举值使用大写字母，例如 `RUNNING`、`SUCCEEDED`。
- 任务状态与 SRS 中的业务状态保持一致。

## 3. Edge 侧任务注入 HTTP API

V1 暂不实现 Cloud Platform，由调用方通过 Edge Server 的 HTTP API 注入任务。后续 Cloud 实现后，本接口仍保留。

- 无鉴权，允许局域网内调用。
- 响应统一使用 `code + msg + data` 结构。

### 3.1 创建/注入任务

`POST /api/tasks`

请求体：

```json
{
  "task_id": "task_001",
  "task_type": "UNLOAD",
  "robot_ids": ["robot_01", "robot_02"],
  "unload_zone_priority": true,
  "goods": [
    {
      "goods_id": "A",
      "goods_name": "A",
      "quantity": 10
    },
    {
      "goods_id": "B",
      "goods_name": "B",
      "quantity": 20
    }
  ]
}
```

字段说明：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `task_id` | string | 是 | 调用方生成的任务唯一标识；Edge 负责幂等处理。 |
| `task_type` | enum | 是 | `UNLOAD` 或 `LOAD`。 |
| `robot_ids` | array[string] | 是 | 本次搬运任务参与的 AMR 列表，不能为空。V1 暂不支持 Edge 自动选择参与 AMR。 |
| `unload_zone_priority` | boolean | 否 | 卸载区货物是否优先搬运，默认 `false`。 |
| `goods` | array | 是 | 货物列表。 |
| `goods[].goods_id` | string | 是 | 货物标识，建议使用 SKU。 |
| `goods[].goods_name` | string | 否 | 货物显示名。 |
| `goods[].quantity` | integer | 是 | 数量，必须大于 0。 |

成功响应：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "task_id": "task_001",
    "state": "PENDING",
    "robot_ids": ["robot_01", "robot_02"]
  }
}
```

重复提交同一 `task_id` 时，不应重复创建任务；应由 Edge 返回已存在任务的状态。

### 3.2 查询任务状态

`GET /api/tasks/{task_id}`

响应：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "task_id": "task_001",
    "state": "RUNNING",
    "robot_ids": ["robot_01", "robot_02"],
    "updated_at": "2026-10-10T10:00:00+08:00"
  }
}
```

### 3.3 取消任务

`POST /api/tasks/{task_id}/cancel`

响应：

```json
{
  "code": 0,
  "msg": "cancel requested",
  "data": {
    "task_id": "task_001",
    "state": "CANCELLED"
  }
}
```

如果任务已经进入终态，Edge 不应错误覆盖已有终态。

## 4. MQTT 任务消息

### 4.1 Topic

| 方向 | Topic | QoS | 说明 |
| --- | --- | --- | --- |
| Cloud → Edge | `warehouse/{warehouse_id}/task` | 1 | Cloud 下发/注入任务，Edge 订阅 `warehouse/+/task`。 |
| Edge → Cloud | `warehouse/{warehouse_id}/task/status` | 1 | Edge 定时或状态变化时上报任务状态。 |
| Edge → AMR | `amr/{robot_id}/task` | 1 | Edge 向指定 AMR 下发任务或取消指令。 |
| Edge → AMR | `warehouse/{warehouse_id}/task/{task_id}/state` | 1 | Edge 发布任务共享状态，所有参与 AMR 订阅。 |
| AMR → Edge | `amr/{robot_id}/task/goods/request` | 1 | AMR 向 Edge 申请搬运指定货物和数量。 |
| Edge → AMR | `amr/{robot_id}/task/goods/response` | 1 | Edge 返回搬运申请结果。 |
| AMR → Edge | `amr/{robot_id}/task/status` | 1 | AMR 定时上报任务状态，以及上报搬运完成、放弃/中断。 |

V1 阶段没有 Cloud，因此这些 Topic 可以先不启用；但 Topic 名称和消息结构必须提前冻结。

### 4.2 Cloud → Edge 任务下发消息

```json
{
  "message_id": "550e8400-e29b-41d4-a716-446655440000",
  "warehouse_id": "warehouse_01",
  "task_id": "task_001",
  "task_type": "UNLOAD",
  "robot_ids": ["robot_01", "robot_02"],
  "unload_zone_priority": true,
  "goods": [
    {
      "goods_id": "A",
      "goods_name": "A",
      "quantity": 10
    },
    {
      "goods_id": "B",
      "goods_name": "B",
      "quantity": 20
    }
  ],
  "created_at": "2026-10-10T10:00:00+08:00"
}
```

字段说明：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message_id` | string | 是 | 消息幂等标识；Edge 用其去重。 |
| `warehouse_id` | string | 是 | 仓库标识，应与 Topic 中一致。 |
| `task_id` | string | 是 | 任务唯一标识。 |
| `task_type` | enum | 是 | `UNLOAD` 或 `LOAD`。 |
| `robot_ids` | array[string] | 是 | 参与搬运任务的 AMR 列表，不能为空。V1 暂不支持 Edge 自动选择参与 AMR。 |
| `unload_zone_priority` | boolean | 否 | 卸载区货物优先搬运。 |
| `goods` | array | 是 | 货物列表。 |
| `created_at` | string | 否 | 任务创建时间，建议 ISO 8601。 |

### 4.3 Edge → AMR 任务下发消息

Edge 根据 `robot_ids` 分别向每个 AMR 下发同一任务。Topic 使用 `amr/{robot_id}/task`，AMR 只订阅自己的 Topic。

```json
{
  "message_id": "550e8400-e29b-41d4-a716-446655440000",
  "warehouse_id": "warehouse_01",
  "task_id": "task_001",
  "task_type": "UNLOAD",
  "unload_zone_priority": true,
  "goods": [
    {
      "goods_id": "A",
      "goods_name": "A",
      "quantity": 10
    },
    {
      "goods_id": "B",
      "goods_name": "B",
      "quantity": 20
    }
  ],
  "total_quantity": 30
}
```

字段说明：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message_id` | string | 是 | Edge 生成的消息幂等标识。 |
| `warehouse_id` | string | 是 | 仓库标识。 |
| `task_id` | string | 是 | 任务唯一标识。 |
| `task_type` | enum | 是 | `UNLOAD` 或 `LOAD`。 |
| `unload_zone_priority` | boolean | 否 | 卸载区货物优先搬运。 |
| `goods` | array | 是 | 任务总货物列表，不做按 AMR 平均分配。 |
| `total_quantity` | integer | 是 | 任务需要搬运的总数量。 |

Edge 不向 AMR 下发“该 AMR 搬运多少”的分配结果，只下发任务总量和货物列表。AMR 每次搬运前必须通过货物申请 Topic 向 Edge 申请指定货物和数量，等待 Edge 返回成功后才能开始搬运。

目的地不直接在任务消息中下发。每类货物有固定的装货区、卸载区或存储区，AMR 根据 `goods_id` 和 `task_type` 决定目标点位。

### 4.4 Edge → AMR 任务共享状态消息

Topic 使用 `warehouse/{warehouse_id}/task/{task_id}/state`。该 Topic 是任务级共享 Topic，不是按 AMR 隔离；所有参与该任务的 AMR 都应订阅。

```json
{
  "warehouse_id": "warehouse_01",
  "task_id": "task_001",
  "state": "RUNNING",
  "total_quantity": 30,
  "in_progress_quantity": 10,
  "completed_quantity": 5,
  "pending_quantity": 15,
  "goods_status": [
    {
      "goods_id": "A",
      "goods_name": "A",
      "total_quantity": 10,
      "in_progress_quantity": 8,
      "completed_quantity": 2,
      "pending_quantity": 0
    },
    {
      "goods_id": "B",
      "goods_name": "B",
      "total_quantity": 20,
      "in_progress_quantity": 2,
      "completed_quantity": 3,
      "pending_quantity": 15
    }
  ],
  "updated_at": "2026-10-10T10:01:00+08:00"
}
```

字段说明：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `warehouse_id` | string | 是 | 仓库标识。 |
| `task_id` | string | 是 | 任务唯一标识。 |
| `state` | enum | 是 | `PENDING`、`ACCEPTED`、`RUNNING`、`SUCCEEDED`、`FAILED`、`CANCELLED`、`INTERRUPTED`。 |
| `total_quantity` | integer | 是 | 任务总搬运数量。 |
| `in_progress_quantity` | integer | 是 | 所有 AMR 正在搬运中的数量。 |
| `completed_quantity` | integer | 是 | 已完成搬运数量。 |
| `pending_quantity` | integer | 是 | 尚未开始搬运数量。 |
| `goods_status` | array | 是 | 每个货物的详细搬运状态。 |
| `goods_status[].goods_id` | string | 是 | 货物标识。 |
| `goods_status[].goods_name` | string | 否 | 货物显示名。 |
| `goods_status[].total_quantity` | integer | 是 | 该货物总数量。 |
| `goods_status[].in_progress_quantity` | integer | 是 | 该货物正在搬运的数量。 |
| `goods_status[].completed_quantity` | integer | 是 | 该货物已完成数量。 |
| `goods_status[].pending_quantity` | integer | 是 | 该货物待搬运数量。 |
| `updated_at` | string | 是 | 状态更新时间。 |

不变式：

- 对每个货物：`in_progress_quantity + completed_quantity + pending_quantity = total_quantity`。
- 顶层 `total_quantity`、`in_progress_quantity`、`completed_quantity`、`pending_quantity` 分别是所有 `goods_status` 对应字段之和。
- `goods_status` 必须包含任务中的所有货物，不能只包含有进度的货物。

Edge 负责聚合各 AMR 上报结果并维护该共享状态。

### 4.5 AMR → Edge 货物搬运申请与响应

AMR 每次搬运前必须向 Edge 申请货物和数量，等待响应成功后才能开始。申请 Topic 和响应 Topic 都按 `robot_id` 隔离。

申请消息：

```json
{
  "request_id": "req_001",
  "warehouse_id": "warehouse_01",
  "task_id": "task_001",
  "robot_id": "robot_01",
  "goods": [
    {
      "goods_id": "A",
      "goods_name": "A",
      "quantity": 15
    },
    {
      "goods_id": "B",
      "goods_name": "B",
      "quantity": 5
    }
  ]
}
```

字段说明：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `request_id` | string | 是 | AMR 生成的申请幂等标识。 |
| `warehouse_id` | string | 是 | 仓库标识。 |
| `task_id` | string | 是 | 任务唯一标识。 |
| `robot_id` | string | 是 | 申请机器人。 |
| `goods` | array | 是 | 本次申请的货物列表。 |
| `goods[].goods_id` | string | 是 | 货物标识。 |
| `goods[].goods_name` | string | 否 | 货物显示名。 |
| `goods[].quantity` | integer | 是 | 申请搬运数量，必须大于 0，且不能超过该货物 `pending_quantity`。 |

响应消息：

```json
{
  "request_id": "req_001",
  "warehouse_id": "warehouse_01",
  "task_id": "task_001",
  "robot_id": "robot_01",
  "goods": [
    {
      "goods_id": "A",
      "goods_name": "A",
      "quantity": 15,
      "accepted": true,
      "reason": ""
    },
    {
      "goods_id": "B",
      "goods_name": "B",
      "quantity": 5,
      "accepted": true,
      "reason": ""
    }
  ],
  "accepted": true,
  "reason": ""
}
```

字段说明：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `request_id` | string | 是 | 与申请消息一致。 |
| `warehouse_id` | string | 是 | 仓库标识。 |
| `task_id` | string | 是 | 任务唯一标识。 |
| `robot_id` | string | 是 | 申请机器人。 |
| `goods` | array | 是 | 本次申请的货物处理结果。 |
| `goods[].goods_id` | string | 是 | 货物标识。 |
| `goods[].goods_name` | string | 否 | 货物显示名。 |
| `goods[].quantity` | integer | 是 | 申请数量。 |
| `goods[].accepted` | boolean | 是 | 该货物是否批准。 |
| `goods[].reason` | string | 否 | 该货物拒绝原因。 |
| `accepted` | boolean | 是 | 整次申请是否全部批准。 |
| `reason` | string | 否 | 拒绝原因。 |

规则：

- AMR 收到整次 `accepted=true` 前不得开始搬运。
- AMR 申请数量必须参考 Edge 最新 `goods_status` 中的 `pending_quantity`，不得超过剩余待搬运数量。
- Edge 对每个货物分别判断：只要 `quantity <= pending_quantity` 即批准。
- Edge 批准后，应立即对每个批准货物执行 `pending_quantity -= quantity`，`in_progress_quantity += quantity`。
- Edge 使用 `request_id + robot_id` 去重，避免重复批准同一申请。
- 正在搬运该货物的 AMR 列表属于 Edge 内部状态，不进入 `goods_status`，也不下发给其他 AMR。

### 4.6 AMR → Edge 任务状态、完成和放弃/中断上报

AMR 接受任务后，通过 `amr/{robot_id}/task/status` 定时上报自身任务状态。完成搬运和放弃已申请数量也复用该 Topic，使用 `event_type` 区分。

AMR 任务状态每 5 秒上报一次。Edge 在 5 分钟内未收到该 AMR 的任何更新时，应将该 AMR 视为异常。

通用消息结构：

```json
{
  "message_id": "550e8400-e29b-41d4-a716-446655440000",
  "warehouse_id": "warehouse_01",
  "task_id": "task_001",
  "robot_id": "robot_01",
  "event_type": "STATUS",
  "task_state": "TRANSPORTING",
  "goods": [],
  "reason": "",
  "timestamp": "2026-10-10T10:02:00+08:00"
}
```

`event_type` 枚举：

| 值 | 用途 |
| --- | --- |
| `STATUS` | AMR 定时上报自身任务状态。 |
| `CARRY_COMPLETED` | AMR 上报某货物已完成搬运数量。 |
| `CARRY_ABORTED` | AMR 上报放弃或取消已申请搬运数量。 |

`task_state` 枚举：

| 值 | 说明 |
| --- | --- |
| `IDLE` | 无任务或等待下一步。 |
| `ACCEPTED` | 已接受任务。 |
| `TRANSPORTING` | 正在搬运货物。 |
| `GRASPING` | 正在抓取货物。 |
| `PLACING` | 正在放置货物。 |
| `INTERRUPTED` | AMR 因电量低、导航失败等原因中断任务。 |

定时状态上报示例：

```json
{
  "message_id": "550e8400-e29b-41d4-a716-446655440000",
  "warehouse_id": "warehouse_01",
  "task_id": "task_001",
  "robot_id": "robot_01",
  "event_type": "STATUS",
  "task_state": "TRANSPORTING",
  "goods": [
    {
      "goods_id": "A",
      "goods_name": "A",
      "quantity": 8
    },
    {
      "goods_id": "B",
      "goods_name": "B",
      "quantity": 2
    }
  ],
  "reason": "",
  "timestamp": "2026-10-10T10:02:00+08:00"
}
```

完成搬运上报示例：

```json
{
  "message_id": "550e8400-e29b-41d4-a716-446655440001",
  "warehouse_id": "warehouse_01",
  "task_id": "task_001",
  "robot_id": "robot_01",
  "event_type": "CARRY_COMPLETED",
  "task_state": "TRANSPORTING",
  "goods": [
    {
      "goods_id": "A",
      "goods_name": "A",
      "quantity": 15
    }
  ],
  "reason": "",
  "timestamp": "2026-10-10T10:05:00+08:00"
}
```

放弃/中断上报示例：

```json
{
  "message_id": "550e8400-e29b-41d4-a716-446655440002",
  "warehouse_id": "warehouse_01",
  "task_id": "task_001",
  "robot_id": "robot_01",
  "event_type": "CARRY_ABORTED",
  "task_state": "INTERRUPTED",
  "goods": [
    {
      "goods_id": "A",
      "goods_name": "A",
      "quantity": 8
    }
  ],
  "reason": "LOW_BATTERY",
  "timestamp": "2026-10-10T10:06:00+08:00"
}
```

字段说明：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message_id` | string | 是 | AMR 生成的消息幂等标识。 |
| `warehouse_id` | string | 是 | 仓库标识。 |
| `task_id` | string | 是 | 任务唯一标识。 |
| `robot_id` | string | 是 | AMR 标识。 |
| `event_type` | enum | 是 | `STATUS`、`CARRY_COMPLETED`、`CARRY_ABORTED`。 |
| `task_state` | enum | 是 | AMR 当前任务状态。 |
| `goods` | array | 条件必填 | `CARRY_COMPLETED` 或 `CARRY_ABORTED` 时必填；`STATUS` 时可空数组或填写当前搬运货物。 |
| `goods[].goods_id` | string | 是 | 货物标识。 |
| `goods[].goods_name` | string | 否 | 货物显示名。 |
| `goods[].quantity` | integer | 是 | 该货物本次完成/放弃或当前搬运数量。 |
| `reason` | string | 否 | `CARRY_ABORTED` 时建议填写，如 `LOW_BATTERY`、`NAVIGATION_FAILED`、`MANUAL_STOP`。 |
| `timestamp` | string | 是 | 上报时间。 |

Edge 处理规则：

- `CARRY_COMPLETED`：对 `goods` 数组中的每个货物，将该货物的 `in_progress_quantity` 减少，`completed_quantity` 增加。
- `CARRY_ABORTED`：对 `goods` 数组中的每个货物，将该货物的 `in_progress_quantity` 减少，`pending_quantity` 增加。
- `STATUS` 用于 Edge 判断 AMR 是否正常执行任务；如果 AMR 长期没有 `STATUS` 上报，Edge 可将其视为异常。
- AMR 也检测 Edge 状态；如果 Edge 异常，AMR 完成当前已批准搬运后停止下一行动，待 Edge 恢复后再补报本地结果。

### 4.7 Edge → Cloud 任务状态消息

```json
{
  "warehouse_id": "warehouse_01",
  "task_id": "task_001",
  "robot_id": "robot_01",
  "state": "RUNNING",
  "reason": "",
  "timestamp": "2026-10-10T10:01:00+08:00"
}
```

字段说明：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `warehouse_id` | string | 是 | 仓库标识。 |
| `task_id` | string | 是 | 任务唯一标识。 |
| `robot_id` | string | 否 | 执行任务的 AMR；未分配时可为空。 |
| `state` | enum | 是 | `PENDING`、`ACCEPTED`、`RUNNING`、`SUCCEEDED`、`FAILED`、`CANCELLED`、`INTERRUPTED`。 |
| `reason` | string | 否 | 失败、取消等状态的原因。 |
| `timestamp` | string | 是 | 状态产生时间。 |

用户原始示例中的 `states` 建议改为单数 `state`；`inProgress` 建议改为 `RUNNING`，以与 SRS 任务状态一致。

关于不可用 AMR：

- `robot_ids` 中存在不可用 AMR 时，如果仍有其他 AMR 可用，任务继续执行，不触发任务级中断。
- 当参与任务的所有 AMR 均异常时，Edge 通过 `warehouse/{warehouse_id}/task/status` 向 Cloud 上报 `state=INTERRUPTED`，`reason=ALL_AMRS_ABNORMAL`。

### 4.8 QoS 与幂等

- 任务下发和任务状态消息使用 QoS 1，保证至少送达一次。
- QoS 1 只解决传输可靠性，不解决业务幂等。
- Edge 必须根据 `task_id` + `message_id` 去重，避免重复创建或重复状态覆盖。
- 任务状态更新应使用 `task_id` + `state` + `timestamp` 判断是否可覆盖，终态不应被旧消息回退。

### 4.9 Edge 故障恢复策略

Edge 异常或与 AMR 的 MQTT 链路中断时，AMR 采用“完成当前已获批搬运，然后安全停止并等待恢复”的策略。

具体规则：

1. AMR 同时监控：
   - MQTT 连接状态；
   - `edge_server/status` 心跳；
   - Edge 状态消息是否超时。
2. `edge_server/status` 使用 retained 消息，保证 AMR 重连后能获取 Edge 最新状态。
3. 当 AMR 判定 Edge 不可用时：
   - 若正在搬运已获批货物，允许完成当前次搬运；
   - 不再发起新的 `task/goods/request`；
   - 将 `CARRY_COMPLETED` 或 `CARRY_ABORTED` 结果写入本地待补报文件；
   - 进入安全等待状态。
4. AMR 本地待补报结果持久化到 `pending_task_reports.json`，采用原子写入，避免进程重启丢失。
5. Edge 恢复后：
   - 从 SQLite 恢复任务、`goods_status` 和 AMR 状态；
   - 接收并处理 AMR 重传的待补报结果；
   - 使用 `message_id + task_id + robot_id + event_type` 去重；
   - 根据 `timestamp` 避免旧消息覆盖新状态；
   - 合并完成后发布最新 `warehouse/{warehouse_id}/task/{task_id}/state`。
6. Edge 恢复合并后的状态以 Edge 持久化数据为权威，AMR 待补报记录在 Edge 确认后可清理。

多 AMR 场景下，Edge 故障期间不进行本地平均分配，避免多个 AMR 重复领取同一批货物。

## 5. AMR 内部 ROS2 接口名称

AMR 内部 Topic/Service/Action 名称统一定义在：

`robot_ws/src/amr_common/include/amr_common/topic_names.h`

| 类型 | 常量 | 名称 |
| --- | --- | --- |
| Topic | `amr_common::topic::MAP_INFO` | `map_info` |
| Topic | `amr_common::topic::ROBOT_STATUS` | `robot_status` |
| Topic | `amr_common::topic::TASK_STATUS` | `task_status` |
| Topic | `amr_common::topic::ROBOT_POSE` | `robot_pose` |
| Service | `amr_common::service::GET_ROBOT_POSE` | `get_robot_pose` |
| Action | `amr_common::action::AMR_NAVIGATE_TO_POSE` | `amr_navigate_to_pose` |

AMR 内部所有名称使用相对名，在多机器人命名空间下解析。

## 6. MQTT Topic 常量

MQTT Topic 统一定义在：

`common/mqtt/mqtt_topic.h`

其中包含：

- `WAREHOUSE_TASK = "warehouse/{}/task"`
- `WAREHOUSE_TASK_STATUS = "warehouse/{}/task/status"`
- `AMR_TASK = "amr/{}/task"`
- `WAREHOUSE_TASK_STATE = "warehouse/{}/task/{}/state"`
- `AMR_TASK_GOODS_REQUEST = "amr/{}/task/goods/request"`
- `AMR_TASK_GOODS_RESPONSE = "amr/{}/task/goods/response"`
- `AMR_TASK_STATUS = "amr/{}/task/status"`
- 现有注册、状态、Will、地图、交通路权等 Topic。

后续实现时优先使用 helper：

- `makeWarehouseTaskTopic(warehouse_id)`
- `makeWarehouseTaskStatusTopic(warehouse_id)`
- `makeAmrTaskTopic(robot_id)`
- `makeWarehouseTaskStateTopic(warehouse_id, task_id)`
- `makeAmrTaskGoodsRequestTopic(robot_id)`
- `makeAmrTaskGoodsResponseTopic(robot_id)`
- `makeAmrTaskStatusTopic(robot_id)`

## 7. TBD

- 是否需要独立 ACK Topic，还是仅依赖 QoS 1 + 幂等字段。
- `unload_zone_priority` 是否应扩展为更通用的优先级规则。
- `task_state` 的完整状态机和超时判定规则仍待细化。

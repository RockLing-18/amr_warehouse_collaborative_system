# 接口契约（V1 / V2）

> **⚠️ 2026-11-22 V0.20 定案变更**：V2 窄道路权以项目所有者 8 项拍板为唯一权威口径，见 [定案记录](../../.workbuddy/artifacts/traffic-gate-8-decisions-frozen.md)（`.workbuddy/artifacts/traffic-gate-8-decisions-frozen.md`）。旧 `NARROW` zone / `TRAFFIC_WAIT` / 有向触发段 / 外移释放线 / `crossed()` 主判据 / `plan` 预判 / `D_trigger` 口径**已废弃**，不得据此实现；§8 已同步为「独立 `traffic_gates.yaml` + 4 点通行控制区 + 中心点进出边沿 + 无 `plan` 订阅」，对应 SRS V0.20。

## 1. 目的与状态

本文档记录 V1 已确定或待冻结的接口约束，是后续 Edge、AMR、Simulation 和任务相关代码开发的接口依据。

当前状态：Draft。

> **V2 补充（2026-11-20 第七轮）**：第 8 节登记了 V2 路权 / 电量接口的**已定名** topic、消息字段与 JSON 编码（INV-14 / SRS D-057~D-059）。第 1~7 节仍为 V1 契约，未改动语义。
>
> **V0.20 补充（2026-11-22）**：第 8 节新增 `traffic_gates.yaml` 完整数据结构、`MapInfo.traffic_gates_yaml` 字段与打包下发链路、validator 内容校验；`TrafficRequest.msg` / `TrafficResponse.msg` 字段集合**保持不变**，不新增 `plan` 相关订阅或字段。

已确定：

- V1 使用 Edge 侧 HTTP API 进行任务注入。
- 后续 Cloud 使用 MQTT 向 Edge 发布任务，Edge 订阅任务 Topic。
- 任务相关 MQTT Topic 使用 `warehouse/{warehouse_id}/task`、`warehouse/{warehouse_id}/task/status`、`amr/{robot_id}/task`、`warehouse/{warehouse_id}/task/{task_id}/state`、`amr/{robot_id}/task/goods/request`、`amr/{robot_id}/task/goods/response`、`amr/{robot_id}/task/status` 和 `amr/{robot_id}/task/status/ack`。
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
    "warehouse_id": "warehouse_01",
    "task_type": "UNLOAD",
    "state": "RUNNING",
    "robot_ids": ["robot_01", "robot_02"],
    "goods": [
      {"goods_id": "A", "goods_name": "A", "quantity": 10},
      {"goods_id": "B", "goods_name": "B", "quantity": 20}
    ],
    "goods_status": [
      {"goods_id": "A", "goods_name": "A", "total_quantity": 10,
       "in_progress_quantity": 0, "completed_quantity": 10, "pending_quantity": 0},
      {"goods_id": "B", "goods_name": "B", "total_quantity": 20,
       "in_progress_quantity": 10, "completed_quantity": 5, "pending_quantity": 5}
    ],
    "total_quantity": 30,
    "in_progress_quantity": 10,
    "completed_quantity": 15,
    "pending_quantity": 5,
    "reason": "first goods approved",
    "updated_at": "2026-10-10T10:00:00+08:00"
  }
}
```

字段说明：

- `goods`：任务静态货物定义（总量）。
- `goods_status`：每个货物的搬运进度（`total_quantity = in_progress_quantity + completed_quantity + pending_quantity`）。
- `total_quantity` / `in_progress_quantity` / `completed_quantity` / `pending_quantity`：任务级汇总，等于 `goods_status` 对应字段之和。

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
| Edge → AMR | `amr/{robot_id}/task/status/ack` | 1 | Edge 确认已处理的 AMR 状态/完成/放弃消息。 |

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
  "request_id": "req_001",
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
  "request_id": "req_001",
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
| `request_id` | string | 条件必填 | `CARRY_COMPLETED` 或 `CARRY_ABORTED` 时必填，用于定位已批准分配。 |
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

### 4.10 AMR 状态上报 ACK

Edge 处理完 AMR 的 `STATUS`、`CARRY_COMPLETED` 或 `CARRY_ABORTED` 消息后，通过 `amr/{robot_id}/task/status/ack` 发送确认。

```json
{
  "warehouse_id": "warehouse_01",
  "task_id": "task_001",
  "robot_id": "robot_01",
  "acked_message_ids": [
    "550e8400-e29b-41d4-a716-446655440000",
    "550e8400-e29b-41d4-a716-446655440001"
  ],
  "timestamp": "2026-10-10T10:07:00+08:00"
}
```

字段说明：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `warehouse_id` | string | 是 | 仓库标识。 |
| `task_id` | string | 是 | 任务唯一标识。 |
| `robot_id` | string | 是 | 接收 ACK 的 AMR。 |
| `acked_message_ids` | array[string] | 是 | Edge 已确认处理的 AMR 消息 ID 列表。 |
| `timestamp` | string | 是 | ACK 时间。 |

AMR 收到 ACK 后，删除本地 `pending_task_reports.json` 中对应的待补报记录。

### 4.11 AMR 注册消息

AMR 启动后通过 `amr/register/request` 向 Edge 发起注册，Edge 通过 `amr/register/response/{robot_id}` 返回结果。

注册请求：

```json
{
  "robot_id": "robot_01",
  "warehouse_id": "warehouse_01",
  "map_version": "",
  "request_id": "robot_01_1792661398945",
  "timestamp": 1792661398945
}
```

字段说明：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `robot_id` | string | 是 | 机器人业务身份。 |
| `warehouse_id` | string | 是 | 所属仓库。 |
| `map_version` | string | 是 | 本地地图版本，可为空。 |
| `request_id` | string | 是 | 注册请求幂等标识。 |
| `timestamp` | integer | 是 | 注册时间戳（毫秒）。 |

注册响应：

```json
{
  "code": 0,
  "message": "success",
  "robot_id": "robot_01",
  "request_id": "robot_01_1792661398945",
  "map_update": true,
  "map_version": "1.0",
  "map_download_url": "/api/maps/download?warehouse_id=warehouse_01",
  "initial_pose": {
    "x": 0.0,
    "y": 0.0,
    "yaw": 1.57
  }
}
```

字段说明：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `code` | integer | 是 | 0 表示成功。 |
| `message` | string | 否 | 可读结果信息。 |
| `robot_id` | string | 是 | 机器人业务身份。 |
| `request_id` | string | 是 | 与请求一致。 |
| `map_update` | boolean | 是 | 是否需要更新地图。 |
| `map_version` | string | 否 | Edge 当前生效地图版本。 |
| `map_download_url` | string | 否 | 需要更新地图时的下载地址。 |
| `initial_pose` | object | 是 | Edge 分配的初始点位（IDLE 充电点）。 |
| `initial_pose.x` / `initial_pose.y` / `initial_pose.yaw` | double | 是 | 初始位姿。 |

## 5. AMR 内部 ROS2 接口名称

AMR 内部 Topic/Service/Action 名称统一定义在：

`robot_ws/src/amr_common/include/amr_common/ros_interface_names.h`

| 类型 | 常量 | 名称 |
| --- | --- | --- |
| Topic | `amr_common::topic::MAP_INFO` | `map_info` |
| Topic | `amr_common::topic::ROBOT_STATUS` | `robot_status` |
| Topic | `amr_common::topic::TASK_STATUS` | `task_status` |
| Topic | `amr_common::topic::ROBOT_POSE` | `robot_pose` |
| Topic | `amr_common::topic::NAVIGATION_STATUS` | `navigation_status` |
| Topic | `amr_common::topic::BEHAVIOR_STATUS` | `behavior_status` |
| Service | `amr_common::service::GET_ROBOT_POSE` | `get_robot_pose` |
| Action | `amr_common::action::AMR_NAVIGATE_TO_POSE` | `amr_navigate_to_pose` |
| Topic（V2） | `amr_common::topic::TRAFFIC_REQUEST` | `traffic_request` |
| Topic（V2） | `amr_common::topic::TRAFFIC_RESPONSE` | `traffic_response` |
| Topic（V2） | `amr_common::topic::POWER_MODE` | `power_mode` |
| Topic（V2） | `amr_common::topic::POWER_LEVEL` | `power_level` |

AMR 内部所有名称使用相对名，在多机器人命名空间下解析。V2 新增名称与消息字段见第 8 节。

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
- `AMR_TASK_STATUS_ACK = "amr/{}/task/status/ack"`
- 现有注册、状态、Will、地图等 Topic。
- **V2 替换 / 新增**：`AMR_TRAFFIC_REQUEST = "amr/{}/traffic_rights/request"`、`AMR_TRAFFIC_RESPONSE = "amr/{}/traffic_rights/response"`、`AMR_TRAFFIC_SUBSCRIBE = "amr/+/traffic_rights/request"`（见第 8 节）。

后续实现时优先使用 helper：

- `makeWarehouseTaskTopic(warehouse_id)`
- `makeWarehouseTaskStatusTopic(warehouse_id)`
- `makeAmrTaskTopic(robot_id)`
- `makeWarehouseTaskStateTopic(warehouse_id, task_id)`
- `makeAmrTaskGoodsRequestTopic(robot_id)`
- `makeAmrTaskGoodsResponseTopic(robot_id)`
- `makeAmrTaskStatusTopic(robot_id)`
- `makeAmrTaskStatusAckTopic(robot_id)`
- **V2 新增** `makeAmrTrafficRequestTopic(robot_id)` / `makeAmrTrafficResponseTopic(robot_id)`。

## 7. TBD

- `unload_zone_priority` 是否应扩展为更通用的优先级规则。
- `task_state` 的完整状态机已基本冻结，见 `docs/architecture/task-state-machine.md`。

---

## 8. V2 路权与电量接口（2026-11-20 第七轮冻结；2026-11-22 V0.20 修订）

> **权威来源**：SRS **V0.20** 的 1.6 节 D-057~D-063（D-060 / D-061 / D-063 于 2026-11-22 按 8 项拍板改写）、`version-roadmap.md` INV-14，以及 `.workbuddy/artifacts/traffic-gate-8-decisions-frozen.md`。本节字段与命名**已冻结**，实现者不得自行更改；表结构与数值参数仍属 TBD-020 / 021 / 022 / 038 / 039，见 8.6 与 8.7。
> 实现级流程、状态机与伪代码见 `docs/architecture/roadmap/v2-implementation-spec.md`。**V2 全程不订阅 Nav2 `plan`**：无 `onPlan()` / `onPlanChanged()` / `pathIntersects*` / 改道去抖 / `D_trigger`（已废弃，提前量由 `L_pull` 提供）；改道后驶离控制区由中心点出区边沿释放、停住或断流由 TTL 回收。

### 8.1 已定名常量总表

| 层 | 常量 | 值 / 名称 | 方向 |
| --- | --- | --- | --- |
| ROS2 | `amr_common::topic::TRAFFIC_REQUEST` | `traffic_request` | `amr_behavior_manager` → `amr_agent` |
| ROS2 | `amr_common::topic::TRAFFIC_RESPONSE` | `traffic_response` | `amr_agent` → `amr_behavior_manager` |
| ROS2 | `amr_common::topic::POWER_MODE` | `power_mode` | `amr_behavior_manager` → `amr_power_manager` |
| ROS2 | `amr_common::topic::POWER_LEVEL` | `power_level` | `amr_power_manager` → `amr_behavior_manager` |
| MQTT | `mqtt_topic::AMR_TRAFFIC_REQUEST` | `amr/{}/traffic_rights/request` | AMR → Edge |
| MQTT | `mqtt_topic::AMR_TRAFFIC_RESPONSE` | `amr/{}/traffic_rights/response` | Edge → AMR（单播） |
| MQTT | `mqtt_topic::AMR_TRAFFIC_SUBSCRIBE` | `amr/+/traffic_rights/request` | Edge 订阅 |

**V1 常量替换清单**：删除 `TRAFFIC_RIGHTS_REQ = "amr/traffic_rights/request"` 与 `TRAFFIC_RIGHTS_RSP_PREFIX = "amr/traffic_rights/response/"`（均缺 `robot_id` 层，与四级通配订阅不自洽）；新增上表三个常量与两个 `make*Topic(robot_id)` helper。**不复用 `task_command` / `task_event` 传路权消息。**

### 8.2 ROS2 消息定义

> **实体文件状态（V0.20 明确）**：本节 4 个 V2 `.msg`（`TrafficRequest.msg` / `TrafficResponse.msg` / `PowerMode.msg` / `BatteryState.msg`）目前**尚无实体文件**，仅以文本形式定义于本契约第 8 节与 `v2-implementation-spec.md` §2，V2 实现时创建。V1 的 `MapInfo.msg` 已有实体文件，`traffic_gates_yaml` 为 V0.20 新增字段（见 8.8），不属于本节 4 个待建文件。
>
> **字段集合保持不变**：`TrafficRequest.msg` / `TrafficResponse.msg` 不因 `traffic_gates.yaml` 引入而增删字段；`TrafficResponse` 已有 `queue_seq`，可直接用于多车等待点分散（备选 `queue_seq % N_side`），**不得新增字段**，也不新增 `plan` 相关消息或订阅。

`amr_interfaces/msg/TrafficRequest.msg`：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `stamp` | `builtin_interfaces/Time` | 是 | 消息产生时间（仿真时钟）。 |
| `op` | `string` | 是 | `REQUEST` / `CANCEL` / `RELEASE`；缺失即丢弃并记日志。 |
| `request_id` | `string` | 是 | 幂等键，建议 `{robot_id}-{resource_id}-{seq}`，同一趟同一资源复用。 |
| `robot_id` | `string` | 是 | 与 MQTT topic 中的 `{robot_id}` 交叉校验。 |
| `resource_id` | `string` | 是 | 窄道 / 区域 id，**不是坐标**。 |
| `epoch` | `int64` | 是 | 请求代次；Edge 丢弃 `epoch` 小于当前值的迟到消息。 |
| `eta_sec` | `float64` | 否 | 预计进入时刻（相对仿真时间，秒）；供排队与 lease 生效参考（`op=REQUEST` 时填）。 |
| `lease_ttl_sec` | `float64` | 是 | 权限占用时限；Edge 授权时写入 `lease_expires_at`。 |

`amr_interfaces/msg/TrafficResponse.msg`：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `stamp` | `builtin_interfaces/Time` | 是 | 响应时间。 |
| `request_id` | `string` | 是 | 与请求一致，用于 AMR 侧匹配。 |
| `robot_id` | `string` | 是 | 单播目标。 |
| `resource_id` | `string` | 是 | 与请求一致。 |
| `epoch` | `int64` | 是 | 与请求一致，供 AMR 丢弃陈旧响应。 |
| `result` | `string` | 是 | `GRANTED` / `REJECTED` / `PENDING` / `CANCELLED` / `RELEASED` / `EXPIRED`。 |
| `queue_seq` | `int32` | 是 | `PENDING` 时的 FIFO 序号；无排队填 `-1`。 |
| `lease_expires_at` | `float64` | 否 | 授权时写入的到期时间（仿真时间秒）。 |
| `reason` | `string` | 否 | 拒绝 / 取消 / 到期原因码，空串表示无。 |

`amr_interfaces/msg/PowerMode.msg`（D 方案）：`int32 mode`（`MODE_IDLE=0` / `MODE_WORK=1` / `MODE_CHARGING=2`）+ `builtin_interfaces/Time stamp`。

`amr_interfaces/msg/BatteryState.msg`：`float32 battery`（0~100）+ `bool is_low` + `builtin_interfaces/Time stamp`。

### 8.3 MQTT JSON 编码

字段一律 `snake_case`（SRS D-008）；`stamp` 在 JSON 中写为 `timestamp`（仿真时间秒，浮点）。QoS 1。

上行 `amr/{robot_id}/traffic_rights/request`（三种 `op` 共用同一条 topic，**不采用动作分层 topic**）：

```json
{
  "op": "REQUEST",
  "request_id": "amr_01-crossing_01-0007",
  "robot_id": "amr_01",
  "resource_id": "crossing_01",
  "epoch": 7,
  "eta_sec": 4.2,
  "lease_ttl_sec": 60.0,
  "timestamp": 1732118400.123
}
```

```json
{ "op": "CANCEL",  "request_id": "amr_01-crossing_01-0007", "robot_id": "amr_01", "resource_id": "crossing_01", "epoch": 7, "lease_ttl_sec": 60.0, "timestamp": 1732118403.0 }
{ "op": "RELEASE", "request_id": "amr_01-crossing_01-0007", "robot_id": "amr_01", "resource_id": "crossing_01", "epoch": 7, "lease_ttl_sec": 60.0, "timestamp": 1732118412.5 }
```

下行单播 `amr/{robot_id}/traffic_rights/response`：

```json
{
  "request_id": "amr_01-crossing_01-0007",
  "robot_id": "amr_01",
  "resource_id": "crossing_01",
  "epoch": 7,
  "result": "GRANTED",
  "queue_seq": -1,
  "lease_expires_at": 1732118460.123,
  "reason": "",
  "timestamp": 1732118400.456
}
```

### 8.4 QoS、幂等与动作规则

1. 上下行均 **QoS 1**；重复投递由 `request_id` 幂等吸收，不得产生第二份 lease。
2. Edge 只订阅 `amr/+/traffic_rights/request`；`robot_id` 从 topic 层取出，与 payload `robot_id` 交叉校验，不一致即丢弃并记日志。
3. `op` 为必填：缺失、取值非法、或把三种动作塞进同一 topic 却不带 `op` 的消息一律丢弃。
4. `CANCEL` 是**一等显式消息**，Edge 不得靠「不再重试 / 断线 / 静默」推断取消；排队中取消 = 出队（`CANCELLED`），已授权取消 = 释放 lease + 配额回退。
5. `epoch` 小于该 `(robot_id, resource_id)` 当前值的迟到消息一律丢弃并记录，不得回退状态。
6. `lease_ttl_sec` 到期由 Edge 自动回收（`EXPIRED`）；崩溃 / `cancel` 丢包 / 区内停住 / 位姿断流时，这是最终兜底。改道后驶离通行控制区由「中心点出区边沿」释放，语义由出区 + TTL 承接，不设独立 `plan` 检测。
7. 响应**单播不广播**；路权占用状态不下发给其他 AMR。

### 8.5 lease 状态机

```text
PENDING（排队中，不占资源） --FIFO 原子授权--> GRANTED --> HELD --> RELEASED
   |                                  |          |
   +-- 显式 CANCEL --> CANCELLED      |          +-- 四触发取最早：① 中心点出区边沿 / ② 任务结束 SUCCEEDED、ABORTED / ③ lease_ttl 到期 / ④ 显式 CANCEL
任意态 -- lease_ttl 到期 / 心跳超时 / Last Will --> EXPIRED（自动回收）
```

> 「改道即释放」保留语义、移除独立机制：驶离控制区归 ①，停住或断流归 ③；`zonePolygon()` 保留供 validator / 异常裁决 / 可视化使用，不再被改道检测调用。

### 8.6 建议 DDL（**表最终字段仍属 TBD-020**）

以下为 `v2-implementation-spec.md` 给出的**建议值**，仅用于本地调试；拍板前不得当作冻结接口：

```sql
CREATE TABLE IF NOT EXISTS tbl_resource_lease (
  request_id        TEXT PRIMARY KEY,
  robot_id          TEXT NOT NULL,
  resource_type     TEXT NOT NULL,
  resource_id       TEXT NOT NULL,
  epoch             INTEGER NOT NULL,
  state             TEXT NOT NULL DEFAULT 'PENDING'
                    CHECK(state IN ('PENDING','GRANTED','HELD','RELEASED','EXPIRED','REVOKED','CANCELLED')),
  op                TEXT NOT NULL,
  queue_seq         INTEGER,
  eta_sec           REAL,
  lease_ttl_sec     REAL NOT NULL,
  lease_expires_at  REAL,
  granted_at        REAL,
  released_at       REAL,
  reason            TEXT
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_lease_active
  ON tbl_resource_lease(resource_type, resource_id)
  WHERE state IN ('GRANTED','HELD');
```

`PENDING` 刻意不进部分唯一索引 —— 排队请求不占资源，同一 `resource_id` 可有多个 `PENDING` 共存，由 Edge 按 `queue_seq` 依次授权。Edge 重启后须能从该表重建占用状态与队列，不得留下幽灵排队项。

### 8.7 `traffic_gates.yaml` 数据结构（V0.20 新增，独立文件）

> 窄道几何**不写回** `zones.yaml` / `stations.yaml`；`zone.type: NARROW`、`TRAFFIC_WAIT` station、成对有向触发段 / 外移释放线 / `side` / `forward` / `trigger` / `release` 等 V0.19 口径**已废弃**，不得实现。数据由工具侧 `traffic_gates_editor` 两阶段产出（阶段 A 录 4 点 zone，阶段 B 录 N 个等待点并按几何自动归档到 `l1` / `l2`），复用 `ClickedPointReceiver` / `ZoneMarkerPublisher`，不复用 `zone_editor` 节点本体 / `YamlZoneManager` / `ZoneInfo` / `selectZoneType()`，不扩展 zone / station type 菜单。

文件草案（顶层列表、点序与字段冻结）：

```yaml
traffic_gates:
  - resource_id: crossing_01
    polygon:                        # 4 点凸四边形；点序冻结：P1→P2 端边 L1 / P2→P3 长轴 / P3→P4 端边 L2
      - [x1, y1]                    # P1
      - [x2, y2]                    # P2
      - [x3, y3]                    # P3
      - [x4, y4]                    # P4
    wait_poses:
      l1:                           # L1 端边外侧（上游），>=1 个
        - {x: .., y: .., yaw: ..}
      l2:                           # L2 端边外侧（上游），>=1 个
        - {x: .., y: .., yaw: ..}
```

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `traffic_gates` | array | 是 | 顶层列表，每个元素对应一条窄道通行控制区。 |
| `traffic_gates[].resource_id` | string | 是 | 窄道 / 区域 id，全局唯一非空，与 `TrafficRequest.resource_id` 一致，**不是坐标**。 |
| `traffic_gates[].polygon` | array[array[float]] | 是 | 恰好 4 个 `[x, y]` 点，凸四边形、点序连续、面积 > 0；点序约定 `P1→P2` 为端边 L1、`P2→P3` 为长轴、`P3→P4` 为端边 L2。 |
| `traffic_gates[].wait_poses.l1` | array[pose] | 是 | L1 端边外侧（上游）等待点，≥1 个；元素为 `{x, y, yaw}`，`yaw` 由长轴 `P2→P3` 派生，不人工填写。 |
| `traffic_gates[].wait_poses.l2` | array[pose] | 是 | L2 端边外侧（上游）等待点，≥1 个；同上。 |

**不设字段**（已废弃）：`side` / `forward` / `trigger` / `release`（含有向触发段、外移释放线、`D_trigger`），以及 `type` / `goods_ids`；一份数据正反两向共用，不新建 station、不复用 `WAIT`、不引入 `TRAFFIC_WAIT` 类型。进入侧由运行时按 `long_axis=normalize(P3-P2)`、`centroid=mean(P1..P4)`、`s=dot(enter_pose-centroid, long_axis)` 于进区边沿算一次并**在本次穿越周期锁存**（`s<0→l1`、`s>0→l2`），组内 `argmin dist` 就近选点，等待点位于入口端边外侧（上游）；多车默认组内 `argmin dist` + validator 点间距校验，备选 `queue_seq % N_side`（见 8.2，`TrafficResponse` 已有 `queue_seq`，不新增字段）。侧边异常穿入按 `s` 符号兜底 + `LOG_WARN`。

**几何与主判据**（与 SRS FR-024、定案记录 §1 / §2 / §4 一致）：4 点区域是端边外移 `min_release_clearance`、侧边沿通道拉长 `L_pull` 的**通行控制区**，不是 3×2 物理窄道；`L_pull ≥ v_max×(RTT_拒绝_p99 + T_cancel + T_newgoal_accept) + v_max²/(2·abs(a_dec)) + d_safety`（数值属 TBD-021 / 038，须 V1.5 实测）；端边不压分叉点，保走绕行路的车「从不进区」→ 零申请（S6）。主判据为 AMR **中心点** `point_in_poly(center, polygon)` 的 `inside` 边沿：进区 `false→true` 发 `REQUEST`、出区 `true→false` 发 `RELEASE`；数据源为 `amr_navigate_to_pose` action feedback 的 `current_pose`，**1 Hz `robot_pose` 禁止作主判据**；必须**边沿检测 + 滞回双阈值**（内缩进入 / 外扩离开，TBD-038），方向无关，掉头从入口退出天然触发出区。`crossed()` 已废弃为主判据，仅保留作侧边异常穿入识别辅助。**全程不订阅 Nav2 `plan`**：`onPlan()` / `onPlanChanged()` / `pathIntersects*` / 位姿远离判定 / 改道去抖 / `D_trigger` 均已废弃并移除；「改道即释放」保留语义、移除独立机制（改道后驶离控制区 → 出区边沿释放；停住 / 断流 → TTL 回收），四触发（出区边沿 / 任务结束 / TTL / 显式 `CANCEL`）取最早。`zonePolygon()` 保留供 validator / 异常裁决 / rviz 可视化使用，不再被改道检测调用；不引入 `NavigateThroughPoses` / 独立 `ComputePath` / `FollowPath`。

**`validator.py` 内容校验**（现状 `tool/map_tool/validator.py:59-72` 只检查文件存在，以下 7 条为 V0.20 新增）：

1. `polygon` 恰为 4 点凸四边形、点序连续、面积 > 0；
2. 两条端边近似平行、与长轴近似垂直，间距 `L_pull` 不低于配置下限；
3. 端边与 `walls.yaml` 线段不冲突，端边所在处通道宽度 ≥ 车宽 + 余量；
4. `wait_poses.l1` / `.l2` 各 ≥1，均在对应端边外侧（上游）、不在 `polygon` 内，且同侧相邻等待点间距 ≥ 车宽 + `d_queue_spacing`；
5. 端边到真实窄道出口净距 ≥ `min_release_clearance`；
6. 区域不与分叉点之前的公共通道重叠（保 S6「走绕行路零申请」）；
7. `resource_id` 全局唯一、非空。

### 8.8 `MapInfo.traffic_gates_yaml` 与打包下发链路（V0.20 新增）

V1 已有实体文件 `MapInfo.msg`（`map_info` topic，字段 `warehouse_id` / `map_version` / `map_dir` / `map_yaml` / `zones_yaml` / `stations_yaml` / `ready`）新增 `string traffic_gates_yaml`，与三个 yaml 字段并列，显式声明地图包第 4 个文件，保证地图包版本一致性。`traffic_gates.yaml` 随地图包下发，**不按约定路径隐式读取**。V2 实现时需同步以下 8 处链路：

| # | 位置 | 变更 |
| --- | --- | --- |
| 1 | `tool/map_tool/config.py` 的 `REQUIRED_WAREHOUSE_FILES` | 加入 `traffic_gates.yaml`。 |
| 2 | `tool/map_tool/metadata.py` | 地图包元数据登记该文件。 |
| 3 | `tool/map_tool/package.py` | 打包时纳入该文件。 |
| 4 | `tool/map_tool/validator.py` | 由「只查文件存在」扩展为 8.7 的 7 条内容校验。 |
| 5 | `MapInfo.msg` | 新增 `string traffic_gates_yaml` 字段。 |
| 6 | `map_manager.cpp::publishMapInfo()` | 填充 `traffic_gates_yaml`。 |
| 7 | `amr_behavior_manager` 的 MapInfo 回调 | 接收并转交新 `traffic_gate_loader`。 |
| 8 | 新 `traffic_gate_loader` | 解析 `polygon` / `wait_poses{l1,l2}`（含 `long_axis` / `centroid` 派生）供 `TrafficGate` 使用；读取细节属 TBD-039。 |

> **实体与交付状态**：8.2 的 4 个 V2 `.msg` **尚无实体文件**，仅以文本定义于本契约与 `v2-implementation-spec.md` §2，V2 实现时创建；V1 的 `MapInfo.msg` 已有实体文件，但 `traffic_gates_yaml` 字段**尚未落地**，V2 实现时补入。`traffic_gates.yaml` 实际地图数据、C++ 实现与工具链代码一律未动。**`TrafficRequest.msg` / `TrafficResponse.msg` 字段集合保持不变**，不新增 `plan` 相关订阅或字段。

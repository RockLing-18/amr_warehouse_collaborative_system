# AMR 本地待补报文件格式

## 1. 目的

AMR 与 Edge 通信中断或 Edge 异常时，AMR 需要把未成功送达的 `STATUS`、`CARRY_COMPLETED`、`CARRY_ABORTED` 消息保存到本地。Edge 恢复后，AMR 按本文件中的顺序重传，并在收到 Edge ACK 后删除对应记录。

## 2. 文件位置

建议路径：

```text
/opt/amr/data/amr_runtime/pending_task_reports.json
```

每个 AMR 实例使用自己的目录，避免多 AMR 实例互相覆盖。

## 3. JSON 结构

```json
{
  "version": 1,
  "robot_id": "robot_01",
  "reports": [
    {
      "message_id": "550e8400-e29b-41d4-a716-446655440000",
      "topic": "amr/robot_01/task/status",
      "payload": "{\"message_id\":\"...\",\"warehouse_id\":\"warehouse_01\",\"task_id\":\"task_001\",\"event_type\":\"CARRY_COMPLETED\",\"task_state\":\"TRANSPORTING\",\"goods\":[{\"goods_id\":\"A\",\"goods_name\":\"A\",\"quantity\":15}],\"reason\":\"\",\"timestamp\":\"2026-10-10T10:05:00+08:00\"}"
    },
    {
      "message_id": "550e8400-e29b-41d4-a716-446655440002",
      "topic": "amr/robot_01/task/status",
      "payload": "{\"message_id\":\"...\",\"warehouse_id\":\"warehouse_01\",\"task_id\":\"task_001\",\"event_type\":\"CARRY_ABORTED\",\"task_state\":\"INTERRUPTED\",\"goods\":[{\"goods_id\":\"A\",\"goods_name\":\"A\",\"quantity\":8}],\"reason\":\"LOW_BATTERY\",\"timestamp\":\"2026-10-10T10:06:00+08:00\"}"
    }
  ]
}
```

字段说明：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `version` | integer | 是 | 文件格式版本，当前为 1。 |
| `robot_id` | string | 是 | 所属 AMR。 |
| `reports` | array | 是 | 待补报消息列表，按生成顺序排列。 |
| `reports[].message_id` | string | 是 | AMR 生成的消息幂等标识。 |
| `reports[].topic` | string | 是 | 重传时需要发布的 MQTT Topic。 |
| `reports[].payload` | string | 是 | 待重传的完整 MQTT 消息 JSON 字符串。 |

## 4. 写入规则

- 使用临时文件写入：先写 `pending_task_reports.json.tmp`，成功后原子重命名为正式文件。
- 写入前按 `message_id` 去重，不重复追加。
- 每次写入应完整覆盖或合并保存，避免多个进程同时写。

## 5. 重传与清理

1. AMR 检测到 Edge 恢复后，按 `reports` 顺序逐条重传。
2. Edge 处理成功后，通过 `amr/{robot_id}/task/status/ack` 返回 `acked_message_ids`。
3. AMR 收到 ACK 后，删除文件中对应 `message_id` 的记录。
4. 若文件为空，删除临时文件和正式文件均可，但不得删除目录。

## 6. 与 Edge 合并的关系

- Edge 使用 `message_id + task_id + robot_id + event_type` 去重。
- Edge 根据 `timestamp` 避免旧消息覆盖新状态。
- AMR 本地文件仅用于补传，不承担最终任务状态权威。

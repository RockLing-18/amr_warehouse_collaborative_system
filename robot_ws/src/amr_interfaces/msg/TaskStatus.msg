# TaskStatus
# amr_manager 发布的当前任务状态（每机器人 namespace 下 topic: task_status）

string task_id
string type                     # INBOUND / OUTBOUND
string sku
int32 target_quantity
int32 completed_quantity
int32 processing_quantity
int32 pending_quantity
string source
string destination

int32 STATE_PENDING=0
int32 STATE_ASSIGNED=1
int32 STATE_IN_PROGRESS=2
int32 STATE_COMPLETED=3
int32 STATE_FAILED=4
int32 state
builtin_interfaces/Time stamp

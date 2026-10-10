# AMR Warehouse Collaborative System

基于 ROS2 的智能仓储 AMR 云边协同仿真平台。

## V1 当前范围

V1 以 Gazebo 仿真代替真实硬件，重点验证以下闭环：

- Edge Server：AMR Bootstrap、注册、地图包管理、机器人状态管理；
- AMR：`amr_agent`、`amr_behavior_manager`、`amr_navigation`；
- 通信：Edge ↔ AMR 使用 HTTP + MQTT，Simulation Manager ↔ Edge 使用 WebSocket；
- 任务：V1 暂不实现 Cloud Platform，通过 Edge 侧 HTTP API 任务注入完成运输任务创建、下发、执行和结果反馈；
- 仿真：Simulation Manager 管理仿真 AMR 生命周期，至少支持两个 AMR 实例。

V1 明确不包含真实硬件、真实传感器、机械臂、交通协调、电量管理和完整 WMS/Web 平台。

需求依据见 [docs/requirements/srs.md](docs/requirements/srs.md)。

## 未来愿景

后续版本可在此基础上扩展：

- Cloud Platform：FastAPI / Web 平台、任务调度与多机协同；
- 视觉：RGB-D 相机颜色识别与 3D 定位；
- 执行：机械臂与力反馈抓取；
- AMR 协同：交通管理、电量管理、动态任务分配与优先级出库；
- 更完整的数据可视化和历史任务分析。

这些内容当前仅为愿景，不属于 V1 验收范围。

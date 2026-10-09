# 软件需求规格说明书（SRS）

**项目名称：** AMR 云边协同多机器人协调系统
**英文名称：** Warehouse AMR Cloud-Edge Collaborative System
**文档版本：** V0.3
**当前开发基线：** V1
**文档状态：** Draft（已按当前代码基线校对）
**最后更新：** 2026-10-09
**运行环境：** Ubuntu 22.04、ROS2 Humble、Gazebo 11

---

## 1. 项目概述

### 1.1 项目背景

本项目旨在构建一个基于 ROS2 的智能仓储 AMR（Autonomous Mobile Robot）云边协同多机器人运输系统。

系统由 Cloud Platform、Edge Server、AMR Application 和 Simulation Environment 四个主要部分组成。

Cloud Platform 负责上层业务管理和任务创建；Edge Server 负责仓库级服务、AMR 接入、任务下发及相关数据管理；AMR Application 负责任务接收、行为编排、导航执行及状态反馈；Simulation Environment 负责机器人仿真、生命周期管理以及仿真状态同步。

项目采用模块化架构，将机器人内部的运动执行能力与外部业务通信隔离，为后续扩展多机器人交通管理、电量管理和高级任务调度提供基础。

### 1.2 项目目标

本项目的主要目标如下：

1. 建立 Cloud、Edge、AMR、Simulation 之间职责清晰的系统架构。
2. 实现 AMR 启动初始化、Edge 注册、地图同步和通信建立。
3. 实现由 Cloud 创建任务、Edge 下发任务、AMR 执行任务并反馈结果的完整业务闭环。
4. 使用 ROS2 和 Nav2 实现机器人内部模块协作及自主导航。
5. 使用 HTTP、MQTT 和 WebSocket 实现不同子系统之间的通信。
6. 使用 Gazebo 构建可重复运行的仓储 AMR 仿真环境。
7. 支持 V1 的任务取消，以及通信中断后的当前任务续行和结果补报。
8. 通过需求文档、架构文档、模块设计文档和验证流程建立规范的工程开发方式。

### 1.3 项目定位

本项目定位为具有完整业务闭环和模块化架构的工程实践及求职作品。

V1 优先保证核心功能能够运行、关键通信链路能够验证、主要模块职责明确，而不是一次性实现完整商业仓储系统的所有功能。

### 1.4 术语与约定

| 术语 | 说明 |
| --- | --- |
| AMR | Autonomous Mobile Robot，自主移动机器人。 |
| Cloud Platform | 负责业务管理、运输任务创建和结果查询的上层平台。 |
| Edge Server | 仓库侧独立 C++ 服务，负责 AMR 接入、地图管理、任务下发及状态汇聚。 |
| Simulation Manager | 负责 Gazebo 仿真 AMR 进程、模型及生命周期管理的组件。 |
| `robot_id` | 机器人业务身份，用于 Edge、Cloud 和 AMR 之间关联机器人。 |
| `simulation_instance_id` | 一次仿真运行中的实例标识，用于区分进程或 Gazebo 模型实例，不应与 `robot_id` 混淆。 |
| `warehouse_id` | 仓库标识，用于关联 Edge Server、地图包和机器人所属仓库。 |
| `task_id` | 运输任务唯一标识，跨 Cloud、Edge、AMR 关联同一任务。 |
| QoS | MQTT 或 ROS2 的服务质量策略。 |
| Bootstrap | AMR 启动时从 Edge Server 获取 MQTT 等接入配置的过程。 |

本文档中的“任务管理模块 / 行为编排模块”在 V1 代码包中对应 `amr_behavior_manager`。为避免文档与实现不一致，后续需求统一使用 `amr_behavior_manager` 作为该模块名称。

需求优先级采用 Must / Should / Could 进行区分。Must 为 V1 必须交付的核心能力，Should 为应尽量满足但可说明例外，Could 为可选能力。

---

## 2. 系统范围

### 2.1 系统组成

| 子系统                    | 主要职责                           |
| ---------------------- | ------------------------------ |
| Cloud Platform         | 上层业务管理、运输任务创建、任务状态查询及云边协同      |
| Edge Server            | 仓库级服务、AMR 接入、任务下发、地图管理、机器人状态管理 |
| AMR Application        | 机器人初始化、任务接收、行为编排、导航执行、任务状态管理   |
| Simulation Environment | Gazebo 仿真、模拟 AMR 生命周期管理、仿真状态同步 |

### 2.2 系统边界

#### 2.2.1 Cloud Platform

Cloud Platform 是上层业务管理和运输任务创建入口。

V1 中，Cloud Platform 负责创建运输任务，并将任务信息交给 Edge Server。Edge Server 根据目标 AMR 和任务信息执行任务下发。

Cloud Platform 应能够获取任务的执行状态及最终结果。

V1 不要求实现完整的 WMS 对接、复杂的全局调度算法或完整的 Web 管理界面。

#### 2.2.2 Edge Server

Edge Server 是独立运行的 C++ 服务程序，不依赖 ROS2。

主要职责包括：

* 提供 HTTP API。
* 通过 Cloud-side MQTT Broker 与 Cloud Platform 通信。
* 通过 Edge-side MQTT Broker 与 AMR 通信。
* 提供 AMR Bootstrap 和注册相关服务。
* 管理地图包的上传、下载、查询及激活。
* 接收、保存和下发运输任务。
* 管理 AMR 的基本信息、连接状态和任务状态。
* 接收 AMR 任务结果，并向 Cloud Platform 提供结果查询或状态同步能力。
* 通过 WebSocket 与 Simulation Manager 通信。
* 使用本地数据库和文件系统保存必要的数据。

#### 2.2.3 AMR Application

AMR Application 基于 ROS2 构建，负责机器人自身的业务执行与运动能力。

V1 主要包含：

* `amr_agent`
* `amr_behavior_manager`
* `amr_navigation`

其中：

* `amr_agent` 负责外部通信、初始化、注册、任务接收、状态上报和必要的断网恢复协调。
* `amr_behavior_manager` 负责任务生命周期管理、任务步骤编排、导航调用、取消处理和失败处理。
* `amr_navigation` 负责基于 Nav2 的导航执行。

`amr_traffic_manager` 和 `amr_power_manager` 作为后续扩展模块，V1 不要求实现完整功能。

除上述业务模块外，V1 工程还包含以下支撑模块：

| 模块 | 用途 |
| --- | --- |
| `amr_common` | AMR 内部共用 Topic 名称等常量。 |
| `amr_interfaces` | AMR 内部 ROS2 消息、服务和 Action 接口定义。 |
| `amr_bringup` | AMR 启动、初始位姿设置等启动辅助能力。 |
| `amr_description` | AMR URDF、传感器、控制器及 RViz/仿真描述。 |
| `amr_simulation` | 面向 AMR 的 Gazebo 仿真与控制器配置。 |
| `warehouse_simulation` | 仓储世界和 Gazebo 启动配置。 |
| `simulation_manager` | 管理仿真 AMR 生命周期、模型及 WebSocket 同步。 |
| `warehouse_tool` | 仓储站点、区域和墙体的可视化编辑工具。 |
| `map_tool` | 地图包打包、校验和上传工具。 |

#### 2.2.4 Simulation Environment

Simulation Environment 由 Simulation Manager 和 Gazebo 组成。

Simulation Manager 负责模拟 AMR 的进程生命周期、仿真模型管理、必要的状态同步和控制器状态检查，并通过 WebSocket 与 Edge Server 通信。

Gazebo 负责提供机器人及仓储环境的物理仿真。

Simulation Environment 不参与 AMR 与 Edge Server 之间的 MQTT 业务通信，也不替代 AMR Application 内部的业务逻辑。

### 2.3 V1 范围

V1 的目标是完成一个可运行、可验证的基本运输任务闭环。

必须具备以下能力：

1. AMR 启动并通过 HTTP 获取 Edge Server 的接入配置。
2. AMR 建立 MQTT 通信并完成注册。
3. AMR 检查本地地图版本，并在必要时同步地图。
4. AMR 完成导航初始化并达到可执行任务的状态。
5. Cloud Platform 能够创建基本运输任务。
6. Edge Server 能够接收 Cloud 创建的任务并向指定 AMR 下发。
7. AMR 能够接收任务，并通过 `amr_behavior_manager` 组织任务执行。
8. AMR 能够通过 Nav2 导航前往指定目标点。
9. 系统支持任务取消，并能够反馈取消结果。
10. AMR 与 Edge 通信中断时，允许当前任务在安全条件满足的情况下继续执行。
11. 通信恢复后，AMR 能够补报尚未成功送达的任务状态和结果。
12. Cloud 能够获取任务状态及最终执行结果。
13. Simulation Manager 能够通过 WebSocket 与 Edge Server 同步仿真 AMR 状态。
14. 至少两个模拟 AMR 能够使用不同身份标识进行区分。

### 2.4 V1 暂不实现的内容

以下内容不作为 V1 的完成条件：

* 多机器人交通管理和路权协调。
* 复杂的多机器人全局任务分配与优化调度。
* 交通冲突预测、死锁检测和自动恢复。
* 电量消耗模型、自动充电及充电任务调度。
* 完整的 WMS 对接。
* 完整的 Web 管理界面和数据可视化平台。
* 生产级高可用、集群部署及跨仓库容灾。
* 完整的商业级权限管理、审计及安全认证体系。
* 真实机器人硬件驱动、真实传感器集成及实机验证。
* 复杂的机械臂操作任务。

V1 的多 AMR 仿真用于验证实例管理、身份隔离和基本运行能力，不代表系统已具备多机器人交通协调能力。

### 2.5 假设与依赖

V1 基于以下假设进行设计：

1. V1 的验证环境为 Gazebo 仿真，不连接真实 AMR、真实传感器或真实仓库设备。
2. Cloud Platform 在 V1 中可以是 CLI、HTTP 脚本或最小化的服务端，用于创建任务和查询状态，不要求完整 Web 界面。
3. Edge Server 与 AMR 处于可互通的网络中，Cloud-side EMQX 与 Edge-side EMQX 均可被访问。
4. V1 以单个仓库 `warehouse_01` 和单个 Edge Server 为主要验证范围。
5. AMR 内部多个 ROS2 节点运行在同一仿真机器人命名空间下，并使用相对 Topic/Service/Action 名称。
6. 安全策略在仿真环境中重点验证控制流和状态流，不涉及真实急停、硬件保护或人员安全认证。
7. 若需要将系统迁移到真实机器人，必须重新评估定位、传感器标定、控制器参数、安全停靠和硬件通信需求。

关键外部依赖至少包括：

* Ubuntu 22.04。
* ROS2 Humble。
* Nav2。
* Gazebo 11 及 `gazebo_ros`。
* EMQX 或其他兼容 MQTT Broker。
* C++17 编译工具链、CMake。
* Paho MQTT C++、yaml-cpp、libwebsockets、SQLite。
* Python 3 及地图工具相关依赖。

上述依赖版本、安装方式和启动命令应由构建与部署文档维护。

---

## 3. 系统架构与通信约束

### 3.1 子系统通信关系

| 通信双方                             | 通信方式                          | 主要用途                 |
| -------------------------------- | ----------------------------- | -------------------- |
| Cloud Platform ↔ Edge Server     | HTTP                          | API 调用及业务数据交互        |
| Cloud Platform ↔ Edge Server     | MQTT                          | 任务创建请求、业务事件及状态交互     |
| Edge Server ↔ AMR                | HTTP                          | Bootstrap、地图包下载等请求   |
| Edge Server ↔ AMR                | MQTT                          | 注册、任务下发、任务取消、状态及业务消息 |
| Simulation Manager ↔ Edge Server | WebSocket                     | 仿真机器人状态及生命周期同步       |
| AMR 内部模块                         | ROS2 Topic / Service / Action | 状态发布、请求调用和长时间运行任务执行  |
| Simulation Manager ↔ Gazebo      | Gazebo 相关接口及进程管理              | 仿真模型、仿真进程及运行状态管理     |

具体业务采用 HTTP 还是 MQTT，应在接口设计文档中明确，不要求同一业务同时通过两种方式实现。

### 3.2 MQTT 基础设施

系统存在两套相互独立的 MQTT Broker：

1. **Cloud-side EMQX**：用于 Cloud Platform 与 Edge Server 之间的 MQTT 通信。
2. **Edge-side EMQX**：用于 Edge Server 与 AMR 之间的 MQTT 通信。

两套 Broker 在逻辑和部署上独立，不应在架构设计中合并为同一个 MQTT Broker。

### 3.3 架构约束

1. Edge Server 不依赖 ROS2，不通过 ROS2 Topic、Service 或 Action 与 AMR 通信。
2. AMR 内部模块使用 ROS2 进行通信。
3. `amr_agent` 负责 AMR 对外接入和主要业务通信。
4. Simulation Manager 与 Edge Server 使用 WebSocket，不使用 MQTT 代替这条通信链路。
5. Simulation Environment 与 AMR 的业务执行逻辑保持解耦。
6. V1 不实现交通管理，但应避免将导航模块与未来交通协调逻辑强耦合。
7. 模块之间通过明确的接口交互，避免直接依赖其他模块的内部实现。
8. 新增需求不得擅自改变上述架构约束；确需调整时，应先更新相关设计文档并确认影响范围。

### 3.4 关键接口与消息概览

本节给出 V1 当前需要覆盖的接口范围。详细请求/响应结构、错误码和状态机由接口设计文档定义。

#### 3.4.1 Edge Server HTTP API

| 方法 | 路径 | 主要用途 |
| --- | --- | --- |
| GET | `/api/amr/bootstrap` | AMR 获取仓库标识、Edge-side MQTT 连接参数。 |
| POST | `/api/maps/upload` | 上传地图包，可指定是否上传后立即激活。 |
| GET | `/api/maps/download` | 下载当前生效地图包。 |
| GET | `/api/maps/active` | 查询当前生效地图元数据。 |
| POST | `/api/maps/activate` | 激活指定版本地图。 |

Cloud Platform 与 Edge Server 的任务创建、状态查询等接口可复用 HTTP 或 Cloud-side MQTT，具体方式待接口设计确定。

#### 3.4.2 Edge Server 与 AMR 的 MQTT Topic

| Topic 模式 | 方向 | 主要用途 |
| --- | --- | --- |
| `edge_server/status` | Edge → AMR | 发布 Edge Server 在线状态。 |
| `amr/register/request` | AMR → Edge | AMR 注册请求。 |
| `amr/register/response/{robot_id}` | Edge → AMR | 指定机器人的注册响应。 |
| `amr/{robot_id}/status` | AMR → Edge | AMR 运行状态、心跳和任务状态快照。 |
| `amr/{robot_id}/will` | AMR → Edge | MQTT Last Will，用于标识 AMR 异常离线。 |
| `warehouse/map/request` | AMR → Edge | 地图请求，预留接口。 |
| `amr/traffic_rights/request`、`amr/traffic_rights/response/{robot_id}` | 双向 | 未来交通路权申请/响应，预留接口。 |

任务下发和任务取消的具体 Topic 必须在接口设计文档中补充，并在实现中遵循 `task_id` 幂等约束。

#### 3.4.3 AMR 内部 ROS2 接口

| 类型 | 名称 | 方向 | 主要用途 |
| --- | --- | --- | --- |
| Topic | `map_info` | `amr_agent` → 其他 AMR 节点 | 发布当前地图信息。 |
| Topic | `robot_status` | `amr_behavior_manager` → 其他 AMR 节点 | 发布机器人聚合状态。 |
| Topic | `task_status` | `amr_behavior_manager` → 其他 AMR 节点 | 发布当前任务状态快照。 |
| Topic | `robot_pose` | `amr_navigation` → 其他 AMR 节点 | 发布当前位姿。 |
| Action | `amr_navigate_to_pose` | `amr_behavior_manager` → `amr_navigation` | 提交单点导航目标并获取执行结果。 |
| Service | `get_robot_pose` | `amr_behavior_manager` → `amr_navigation` | 按需查询机器人位姿。 |

所有 AMR 内部接口优先使用相对名称，在机器人命名空间下解析，避免多 AMR 之间的 Topic、节点和 TF 冲突。

---

## 4. 功能需求

### 4.1 AMR 启动与接入

#### FR-001：AMR Bootstrap

**优先级：** Must
**版本：** V1

AMR 启动后，`amr_agent` 应能够通过 HTTP 请求 Edge Server 的 Bootstrap API。

请求至少提供 AMR 的身份标识 `robot_id`。

在 AMR 身份及配置有效的情况下，Edge Server 应返回接入所需的基础配置，至少包括：

* `warehouse_id`
* Edge-side MQTT Broker 地址及端口
* MQTT 连接所需的配置参数

当前生效地图版本或下载地址可在注册响应中返回，而不是要求 Bootstrap 接口一次性包含全部地图信息。若后续实现调整，应同步更新接口设计文档。

请求参数无效、AMR 未注册或服务不可用时，应返回可识别的错误信息。

具体请求字段、响应结构和错误码在接口设计文档中定义。

#### FR-002：AMR 注册

**优先级：** Must
**版本：** V1

AMR 获取接入配置后，应能够连接 Edge-side MQTT Broker，并完成机器人注册。

注册消息应包含识别 AMR 所需的基本信息，至少能够关联：

* `robot_id`
* 所属仓库
* `simulation_instance_id`（仿真场景下需要区分实例）
* 当前运行状态或必要的初始化信息
* 本地地图版本或为空的地图版本信息

Edge Server 应记录注册结果，并维护已接入 AMR 的基本信息。

注册响应应至少包含 `code`、`message`、`robot_id`、`request_id`。当需要同步地图时，还应包含目标 `map_version` 和 `map_download_url`。

注册失败时，AMR 应能够识别失败状态，并根据错误类型进行重试或等待处理。

#### FR-003：通信重连与状态恢复

**优先级：** Must
**版本：** V1

当 AMR 与 Edge Server 之间的 HTTP 或 MQTT 通信暂时中断时，系统应能够识别通信异常。

对于 MQTT 连接中断，AMR 应能够重新连接，并在连接恢复后重新订阅必要的消息。

通信恢复后，AMR 应能够重新同步必要的机器人状态、当前任务状态及尚未成功送达的任务结果。

对于尚未完成的任务，系统应能够识别任务的实际执行状态，避免仅因通信中断而重复执行同一任务。

本需求要求实现基本的重连与结果补报能力，不要求 V1 实现复杂的分布式事务或完整的消息中间件持久化方案。

### 4.2 地图管理与同步

#### FR-004：地图包管理

**优先级：** Must
**版本：** V1

Edge Server 应支持地图包的上传、下载、查询及激活。

地图包应能够关联仓库标识和地图版本。

Edge Server 应能够识别当前生效的地图版本，并保存地图包的必要元数据。

地图包格式、压缩格式、版本命名规则和校验方式在地图接口设计中定义。

#### FR-005：AMR 地图版本检查与同步

**优先级：** Must
**版本：** V1

AMR 启动并获得仓库相关配置后，应能够检查本地地图与 Edge Server 当前生效地图之间的版本关系。V1 可通过注册响应中的 `map_version`、`map_update` 和 `map_download_url` 完成该判断。

当本地地图缺失或版本不一致时，AMR 应能够通过 HTTP 下载所需地图包。

下载完成后，AMR 应对地图包进行必要的完整性检查，并将其保存到约定的本地地图目录。

只有在地图包下载及必要的校验成功后，才应将其视为可用地图。

地图下载失败时，不应将不完整的地图包直接作为有效地图使用。

地图下载、校验、切换和 Nav2 加载之间的详细流程在设计文档中定义。

### 4.3 AMR 导航初始化

#### FR-006：导航初始化

**优先级：** Must
**版本：** V1

AMR 应能够在地图准备完成后完成导航所需的初始化。

初始化至少涉及：

* 地图文件及导航配置准备。
* Nav2 相关节点启动和就绪检查。
* 定位初始化及初始位姿设置。
* 必要的 TF 和导航状态检查。

导航系统未就绪时，不应将 AMR 标记为可执行导航任务。

导航初始化的执行主体、节点启动时序及错误恢复策略在 AMR 设计文档中定义。

### 4.4 Cloud 与 Edge 任务管理

#### FR-007：Cloud 创建运输任务

**优先级：** Must
**版本：** V1

Cloud Platform 应作为 V1 运输任务的创建入口。

Cloud 创建任务后，应将任务信息交给 Edge Server，由 Edge Server 负责后续任务下发。

基本运输任务至少应包含：

* 唯一任务标识 `task_id`
* 目标 AMR 标识 `robot_id`，或能够用于后续指定目标 AMR 的必要信息
* 任务类型
* 目标位置或导航目标点
* 必要的任务参数

Cloud 与 Edge 应能够识别同一个任务，避免在跨系统交互时产生无法关联的任务记录。

V1 暂不要求 Cloud 实现复杂的自动任务分配算法。任务目标 AMR 的选择方式、任务创建接口和异常处理规则在后续设计中明确。

#### FR-008：Edge 任务接收与下发

**优先级：** Must
**版本：** V1

Edge Server 应能够接收 Cloud 创建的任务，校验必要的任务参数，并将任务下发至目标 AMR。

Edge Server 应维护任务的基本状态，并能够识别任务是否已下发、是否已被 AMR 接受，以及当前执行结果。

当目标 AMR 不可用、任务参数无效或下发失败时，Edge Server 应能够记录失败原因，并向 Cloud 提供可识别的处理结果。

Edge Server 不应将“消息已发送”等同于“任务已成功执行”。

### 4.5 AMR 任务执行

#### FR-009：任务接收与执行编排

**优先级：** Must
**版本：** V1

`amr_agent` 接收到任务消息后，应对消息进行必要的格式及参数校验，并将有效任务交给 `amr_behavior_manager` 处理。

`amr_behavior_manager` 负责：

* 管理任务执行生命周期。
* 按照既定流程组织任务步骤。
* 调用导航能力执行目标点移动。
* 处理导航成功、失败及取消等结果。
* 根据执行结果更新任务状态。
* 执行必要的失败处理。
* 处理任务执行期间的取消请求。

行为树或等价的状态机/流程编排机制可用于实现任务步骤编排、条件判断和失败分支。

`amr_behavior_manager` 不负责替代 Nav2 的路径规划与运动控制，也不直接承担 Cloud 的任务创建或 Edge 的任务下发职责。

#### FR-010：导航目标执行

**优先级：** Must
**版本：** V1

`amr_navigation` 应基于 Nav2 提供导航执行能力。

`amr_behavior_manager` 应能够通过明确的 ROS2 接口向 `amr_navigation` 提交目标位置，并获得导航执行结果。

导航模块应能够区分至少以下结果：

* 导航成功。
* 导航失败。
* 导航被取消。

当导航失败时，`amr_behavior_manager` 应能够依据返回结果决定任务后续处理方式。

V1 只要求基本导航，不要求实现交通冲突检测、路权申请、交通等待和多机器人协同路径规划。

#### FR-011：任务状态管理

**优先级：** Must
**版本：** V1

系统应维护基本的任务状态。

建议的状态集合如下：

| 状态          | 含义        |
| ----------- | --------- |
| `PENDING`   | 任务等待下发或执行 |
| `ACCEPTED`  | AMR 已接受任务 |
| `RUNNING`   | 任务正在执行    |
| `SUCCEEDED` | 任务成功完成    |
| `FAILED`    | 任务执行失败    |
| `CANCELLED` | 任务已取消     |

状态转换应受到约束，不应出现无法解释的状态跳转。

V1 至少应支持以下基本状态流转：

* 正常执行：`PENDING` → `ACCEPTED` → `RUNNING` → `SUCCEEDED`
* 执行失败：`RUNNING` → `FAILED`
* 执行取消：任务进入执行阶段后，收到有效取消请求，完成必要的取消处理后进入 `CANCELLED`

系统应区分任务的业务状态与 MQTT 消息的发送状态。通信失败不得直接将业务任务标记为失败。

任务取消后的状态一致性、重复请求处理及终态任务的重新执行规则，应在接口设计中进一步明确。

#### FR-012：任务取消

**优先级：** Must
**版本：** V1

系统应支持对已接受或正在执行的运输任务发起取消请求。

取消请求应能够从上层业务经 Edge Server 传递至目标 AMR。

AMR 收到有效取消请求后，应由 `amr_behavior_manager` 协调取消当前任务，并在适当情况下通过导航接口取消正在执行的导航动作。

取消处理应满足以下要求：

1. 取消请求能够关联正确的 `task_id`。
2. 不应因重复收到同一取消请求而重复执行取消操作。
3. 系统应区分“取消请求已收到”和“任务已成功取消”。
4. 只有在任务取消处理完成后，才能将任务状态更新为 `CANCELLED`。
5. AMR 应将取消结果上报 Edge Server。
6. Edge Server 应更新任务状态，并使 Cloud 能够获取取消结果。
7. 如果任务已经成功完成或已经失败，应根据既定状态规则处理取消请求，不得错误地覆盖原有终态。

任务取消应优先通过正常的任务控制流程实现，不得以直接终止 ROS2 节点或强制结束机器人进程作为常规取消方式。

任务取消时的安全停止策略及具体状态转换，应在 `amr_behavior_manager` 和导航接口设计中定义。

### 4.6 通信中断与任务连续性

#### FR-013：通信中断期间继续执行当前任务

**优先级：** Must
**版本：** V1

当 AMR 与 Edge Server 之间的 MQTT 通信中断时，如果当前任务已经被 AMR 接受并开始执行，且机器人自身运行条件、安全状态和任务条件允许，则 AMR 可以继续执行当前任务。

通信中断本身不得自动导致正在执行的任务失败或被取消。

AMR 应在本地维护当前任务的必要状态，以便在通信恢复后继续进行状态同步。

本需求不代表机器人可以忽略安全故障、导航故障或其他必须停止执行的异常。机器人自身的安全和故障处理优先于任务连续性。

V1 不要求在通信中断期间继续接收和执行新的远程任务。

#### FR-014：通信恢复后的任务状态补报

**优先级：** Must
**版本：** V1

当 AMR 与 Edge Server 的 MQTT 通信恢复后，AMR 应能够重新同步当前任务状态及尚未成功送达的任务结果。

至少应满足：

1. AMR 能够识别通信中断期间发生的任务状态变化。
2. AMR 能够补报任务成功、失败或取消等最终结果。
3. 补报消息应包含能够关联任务和机器人的标识。
4. Edge Server 应能够处理重复收到的状态消息，不得因重复消息重复创建任务或重复执行任务。
5. Edge Server 应更新任务的最新有效状态。
6. Cloud 应能够通过 Edge Server 获取最终任务结果。

V1 可以采用轻量级的本地待发送结果记录和重连后补报机制，不要求实现完整的分布式事务系统。

具体的消息确认、补报顺序、记录清理和状态冲突处理策略在接口设计文档中定义。

### 4.7 机器人状态管理

#### FR-015：AMR 状态上报

**优先级：** Must
**版本：** V1

AMR 应能够向 Edge Server 上报基本运行状态。

状态信息至少应支持识别：

* `robot_id`
* 当前连接状态或最后已知连接状态
* 当前任务标识及任务状态（如有）
* 必要的状态更新时间

Edge Server 应能够区分不同 AMR，并维护最新的有效状态。

对于位置、电量、故障码等扩展状态，可根据 V1 实际实现情况逐步补充。

### 4.8 仿真管理

#### FR-016：仿真 AMR 生命周期管理

**优先级：** Must
**版本：** V1

Simulation Manager 应能够管理仿真环境中的 AMR 实例，包括启动、运行状态检查及必要的生命周期管理。

每个仿真 AMR 应具有可区分的身份信息，包括：

* `robot_id`
* `simulation_instance_id`

Gazebo 中的模型名称应能够根据机器人身份进行区分，避免多个仿真实例使用相同模型名称造成冲突。

#### FR-017：仿真状态同步

**优先级：** Must
**版本：** V1

Simulation Manager 应通过 WebSocket 与 Edge Server 交换仿真 AMR 相关状态。

系统应能够识别仿真 AMR 的新增、运行状态变化及必要的生命周期事件。

仿真状态同步不应依赖 AMR 与 Edge Server 之间的 MQTT 业务链路。

#### FR-018：多 AMR 仿真

**优先级：** Must
**版本：** V1

系统应至少支持两个模拟 AMR 在同一仿真环境中使用不同身份运行。

不同 AMR 应具有相互隔离的 ROS2 命名空间和必要的运行配置，以避免 Topic、节点、TF 和控制器资源发生意外冲突。

V1 验收重点是多 AMR 的身份区分、基本启动和导航能力，不要求实现多机器人交通协调。

### 4.9 Cloud 与 Edge 协同

#### FR-019：Cloud 与 Edge 通信

**优先级：** Must
**版本：** V1

Cloud Platform 应能够通过 HTTP 与 Edge Server 进行必要的 API 交互，并通过 Cloud-side MQTT Broker 与 Edge Server 交换业务消息。

V1 应验证 Cloud 创建运输任务、Edge 接收任务、Edge 下发任务、任务状态反馈和 Cloud 获取最终结果的基本业务流程。

具体哪些交互采用 HTTP、哪些采用 MQTT，应在接口设计文档中确定。

#### FR-020：端到端业务闭环

**优先级：** Must
**版本：** V1

系统应能够验证从 Cloud 创建运输任务到 AMR 执行任务并反馈结果的完整业务流程。

正常执行流程至少包括：

1. Cloud 创建运输任务。
2. Cloud 将任务信息交给 Edge Server。
3. Edge Server 校验并保存任务。
4. Edge Server 将任务下发给目标 AMR。
5. AMR 接收并接受任务。
6. `amr_behavior_manager` 组织任务执行。
7. `amr_navigation` 调用 Nav2 前往目标位置。
8. AMR 获得导航结果并更新任务状态。
9. AMR 将任务状态上报 Edge Server。
10. Edge Server 更新任务状态。
11. Cloud 获取任务最终结果。

取消流程至少包括：

1. Cloud 或其他获授权的上层调用方发起任务取消请求。
2. 请求经 Edge Server 传递至目标 AMR。
3. AMR 协调取消任务及必要的导航动作。
4. AMR 将取消结果反馈 Edge Server。
5. Edge Server 更新任务状态。
6. Cloud 获取取消结果。

通信中断恢复流程至少包括：

1. AMR 与 Edge 的 MQTT 通信中断。
2. AMR 在满足安全条件的前提下继续执行当前任务。
3. AMR 本地记录必要的任务状态变化及待补报结果。
4. MQTT 通信恢复。
5. AMR 重新同步状态并补报结果。
6. Edge Server 更新有效任务状态。
7. Cloud 获取最终结果。

---

## 5. 非功能需求

### NFR-001：模块化与单一职责

**优先级：** Must

各模块应具有清晰的职责边界。

业务编排、导航执行、外部通信、仿真管理和数据持久化不应无序耦合。

模块之间应通过明确的接口进行交互，避免依赖其他模块的内部实现细节。

### NFR-002：通信隔离

**优先级：** Must

AMR 内部使用 ROS2；AMR 与 Edge Server 使用 HTTP 和 MQTT；Simulation Manager 与 Edge Server 使用 WebSocket。

不得为了简化实现而破坏已确定的通信边界。

### NFR-003：可构建性

**优先级：** Must

项目应能够在约定的 Ubuntu 22.04 开发环境中构建。

ROS2 工作空间应能够使用 `colcon build` 构建；独立 C++ 的 Edge Server 应能够使用 CMake 构建。

必要的依赖、构建命令和环境要求应有文档记录。

### NFR-004：可测试性

**优先级：** Must

核心模块应能够进行独立测试或通过可重复的集成流程验证。

至少应能够验证：

* HTTP 接口。
* MQTT 消息交互。
* 地图包管理与同步。
* AMR 导航执行。
* 任务状态流转。
* 任务取消。
* 通信恢复及任务结果补报。
* WebSocket 仿真状态同步。
* 多 AMR 身份隔离。

### NFR-005：故障可诊断性

**优先级：** Must

系统应对启动失败、通信异常、地图同步失败、导航失败、任务取消失败和任务执行失败提供必要的日志信息。

日志应包含足以定位问题的模块标识、任务标识、机器人标识和错误原因。

### NFR-006：可维护性

**优先级：** Must

C++ 模块应具有合理的类职责、接口边界和依赖关系。

重要的构建配置、运行配置及第三方依赖应有明确的管理方式。

### NFR-007：可扩展性

**优先级：** Should

架构应为后续增加交通管理、电量管理、复杂任务调度和 WMS 对接预留合理的扩展空间。

预留扩展能力不代表必须在 V1 中实现相应业务功能。

### NFR-008：网络异常处理

**优先级：** Must

通信失败不应被静默地当作业务成功。

系统应能够识别关键通信异常，并根据具体业务采取重试、等待或报告失败等处理方式。

对于执行中的任务，V1 应支持在通信中断时继续执行当前任务，并在恢复连接后补报结果。

### NFR-009：配置与敏感信息管理

**优先级：** Must

运行环境相关的 IP、端口、账号和其他配置应与业务代码分离。

真实密码、访问令牌和其他敏感信息不得直接提交到公开 GitHub 仓库。

### NFR-010：任务状态一致性

**优先级：** Must

Cloud、Edge 和 AMR 应能够通过 `task_id` 关联同一个任务。

系统应区分消息发送、消息接收、任务接受、任务执行和任务完成等不同阶段。

重复消息不得导致重复执行任务、错误覆盖任务终态或重复创建任务。

通信恢复后的状态同步应尽可能收敛到能够解释的最终任务状态。发生无法自动判定的状态冲突时，应记录异常并提供必要的诊断信息。

### NFR-011：任务取消的可控性

**优先级：** Must

任务取消应通过明确的控制接口执行。

系统应区分取消请求、取消处理中和取消完成等阶段，避免将尚未完成的取消操作错误地报告为成功。

取消流程不得绕过机器人自身必要的安全和故障处理机制。

### NFR-012：仿真可复现性

**优先级：** Must

V1 使用仿真代替实际硬件，仿真环境和启动参数应可重复执行。

至少应满足：

* 相同仓库、地图和启动配置可以重复启动仿真环境。
* 仿真 AMR 的 `robot_id`、`simulation_instance_id`、命名空间和 Gazebo 模型名称具有明确规则。
* 地图包、世界文件、控制器参数和启动脚本纳入版本管理。
* 关键验证能够通过固定场景和固定配置复现，避免依赖手工临时修改。

### NFR-013：仿真安全边界

**优先级：** Must

在仿真环境中，安全需求仍应以任务状态、导航取消、进程生命周期和异常处理的可验证性为核心。

系统不得以“仿真环境不会造成真实伤害”为由省略错误处理、状态校验和日志记录。任何后续面向真实机器人的迁移都必须重新进行安全评估。

---

## 6. 核心业务流程

### 6.1 AMR 启动与初始化流程

1. Simulation Manager 或其他启动机制启动 AMR。
2. AMR 启动必要的 ROS2 节点。
3. `amr_agent` 通过 HTTP 请求 Bootstrap 配置。
4. AMR 获取仓库标识及 Edge-side MQTT 连接配置。
5. AMR 连接 MQTT Broker 并完成注册。
6. AMR 检查本地地图版本。
7. 必要时下载并校验地图包。
8. 导航模块完成地图加载、定位初始化和就绪检查。
9. AMR 向 Edge Server 上报可用状态。

### 6.2 正常运输任务流程

1. Cloud 创建运输任务。
2. Cloud 将任务交给 Edge Server。
3. Edge Server 校验并保存任务。
4. Edge Server 将任务下发给指定 AMR。
5. `amr_agent` 接收并校验任务。
6. `amr_behavior_manager` 接管任务执行流程。
7. `amr_behavior_manager` 调用 `amr_navigation` 执行导航。
8. 导航模块返回执行结果。
9. `amr_behavior_manager` 更新任务状态。
10. `amr_agent` 将任务状态上报 Edge Server。
11. Edge Server 保存最新任务状态。
12. Cloud 获取任务执行结果。

### 6.3 任务取消流程

1. 上层业务发起取消请求。
2. Edge Server 校验任务状态并将取消请求转发至目标 AMR。
3. AMR 检查任务标识及任务当前状态。
4. `amr_behavior_manager` 协调任务取消。
5. 必要时调用导航接口取消当前导航动作。
6. AMR 更新任务状态并上报取消结果。
7. Edge Server 保存取消结果。
8. Cloud 获取取消后的任务状态。

如果取消请求到达时任务已经进入终态，系统应根据既定规则返回结果，不应错误地覆盖已有的终态记录。

### 6.4 通信中断与结果补报流程

1. AMR 检测到与 Edge Server 的 MQTT 通信中断。
2. AMR 保留当前任务的必要执行状态。
3. 在安全条件允许的情况下，AMR 继续执行当前任务。
4. 任务执行过程中产生的关键状态变化及最终结果被记录为待同步数据。
5. AMR 检测到 MQTT 通信恢复。
6. AMR 重新建立必要的订阅和通信状态。
7. AMR 补报尚未成功送达的任务状态及结果。
8. Edge Server 依据任务标识处理补报消息，并更新最新有效状态。
9. Cloud 获取最终任务结果。

### 6.5 仿真状态同步流程

1. Simulation Manager 启动或发现仿真 AMR 实例。
2. Simulation Manager 获取必要的实例信息和运行状态。
3. Simulation Manager 通过 WebSocket 将相关状态同步至 Edge Server。
4. Edge Server 更新仿真 AMR 的相关记录。
5. 当实例状态变化或实例退出时，Simulation Manager 同步相应变化。

### 6.6 关键数据对象

V1 至少应维护以下逻辑数据对象：

| 数据对象 | 关键字段 | 说明 |
| --- | --- | --- |
| 机器人注册信息 | `robot_id`、`simulation_instance_id`、`warehouse_id`、`register_timestamp` | Edge Server 维护 AMR 接入信息，区分业务身份与仿真实例。 |
| 机器人运行状态 | `robot_id`、`state`、`online`、`battery`、`pose`、`task_id`、`timestamp` | 用于机器人列表、状态查询和异常识别。 |
| 地图包 | `warehouse_id`、`version`、`package_name`、`package_path`、`package_size`、`upload_time`、`activate` | 记录地图版本、文件位置和激活状态。 |
| 运输任务 | `task_id`、`robot_id`、`task_type`、`goal`、`status`、`created_at`、`updated_at` | 跨 Cloud、Edge、AMR 关联任务，是状态同步和幂等处理的核心。 |
| 任务状态事件 | `task_id`、`status`、`reason`、`timestamp`、`source` | 记录任务状态变化，用于补报、审计和冲突诊断。 |
| 仿真 AMR 实例 | `robot_id`、`simulation_instance_id`、`model_name`、`lifecycle_state` | Simulation Manager 与 Edge Server 之间同步的仿真对象。 |

具体数据库表结构、字段类型和索引设计由数据库/接口设计文档定义。字段在实现中可拆分为更细的表，但必须保持上述业务关联能力。

---

## 7. 验收标准

V1 只有在核心需求得到实际验证后，才能视为完成。

| 验收 ID  | 验收内容          | 验收条件                                           |
| ------ | ------------- | ---------------------------------------------- |
| AC-001 | 开发环境构建        | ROS2 工作空间及 Edge Server 均能按文档完成构建               |
| AC-002 | AMR Bootstrap | AMR 能通过 HTTP 获取必要的接入配置                         |
| AC-003 | AMR 注册        | AMR 能通过 Edge-side MQTT 完成注册，Edge 能识别机器人身份      |
| AC-004 | 地图管理          | Edge 能够管理地图包，并识别当前生效地图                         |
| AC-005 | 地图同步          | AMR 能检查地图版本，并在必要时下载地图包                         |
| AC-006 | 导航初始化         | AMR 能完成导航初始化并报告就绪状态                            |
| AC-007 | Cloud 创建任务    | Cloud 创建任务后，Edge 能接收并识别对应的任务记录                 |
| AC-008 | Edge 任务下发     | Edge 能向指定 AMR 下发任务，AMR 能正确接受任务                 |
| AC-009 | 正常任务执行        | AMR 能通过 `amr_behavior_manager` 调用 Nav2 前往目标位置                    |
| AC-010 | 任务结果反馈        | AMR 能将任务成功或失败状态反馈给 Edge，Cloud 能获取结果            |
| AC-011 | 任务取消          | 对运行中的任务发起取消请求后，AMR 能执行取消流程并反馈最终结果              |
| AC-012 | 通信中断续行        | 执行中的任务在 MQTT 中断期间，能够在安全条件满足时继续执行               |
| AC-013 | 结果补报          | MQTT 恢复后，AMR 能补报待同步结果，Edge 能正确更新任务状态           |
| AC-014 | 重复消息处理        | 重复的任务或状态消息不会导致重复执行任务或错误覆盖终态                    |
| AC-015 | 仿真同步          | Simulation Manager 能通过 WebSocket 与 Edge 同步仿真状态 |
| AC-016 | 多 AMR 仿真      | 至少两个 AMR 实例能够区分身份并运行于同一仿真环境                    |
| AC-017 | 故障诊断          | 关键初始化、通信、导航和任务失败能够产生可识别的日志                     |
| AC-018 | 文档完整性         | 项目具备基本需求、架构、构建及验证文档                            |

### 7.1 需求与验收追溯矩阵

| 需求编号 | 需求内容 | 主要验收项 |
| --- | --- | --- |
| FR-001 | AMR Bootstrap | AC-002 |
| FR-002 | AMR 注册 | AC-003 |
| FR-003 | 通信重连与状态恢复 | AC-012、AC-013 |
| FR-004 | 地图包管理 | AC-004 |
| FR-005 | AMR 地图版本检查与同步 | AC-005 |
| FR-006 | 导航初始化 | AC-006 |
| FR-007 | Cloud 创建运输任务 | AC-007 |
| FR-008 | Edge 任务接收与下发 | AC-007、AC-008 |
| FR-009 | 任务接收与执行编排 | AC-008、AC-009 |
| FR-010 | 导航目标执行 | AC-009 |
| FR-011 | 任务状态管理 | AC-010、AC-014 |
| FR-012 | 任务取消 | AC-011 |
| FR-013 | 通信中断期间继续执行当前任务 | AC-012 |
| FR-014 | 通信恢复后的任务状态补报 | AC-013、AC-014 |
| FR-015 | AMR 状态上报 | AC-003、AC-010 |
| FR-016 | 仿真 AMR 生命周期管理 | AC-015、AC-016 |
| FR-017 | 仿真状态同步 | AC-015 |
| FR-018 | 多 AMR 仿真 | AC-016 |
| FR-019 | Cloud 与 Edge 通信 | AC-007、AC-010 |
| FR-020 | 端到端业务闭环 | AC-007 至 AC-014 |
| NFR-001 至 NFR-013 | 模块化、通信隔离、可构建、可测试、可诊断、可维护、可扩展、网络异常、配置管理、状态一致性、取消可控、仿真可复现、仿真安全 | AC-001、AC-014、AC-016、AC-017、AC-018 |

验收过程中应保留必要的构建输出、测试结果、运行日志或截图，便于后续复现与展示。

---

## 8. 后续版本规划

以下内容暂定为 V2 或更后续版本的规划，不属于 V1 的完成条件。

### 8.1 多机器人交通管理

* 实现 `amr_traffic_manager`。
* 实现机器人间的路权请求与响应。
* 支持等待、恢复和必要的交通异常处理。
* 逐步完善多 AMR 协同运行能力。
* 研究交通管理与导航执行之间的协调机制。

V1 不实现交通路权协调。多 AMR 仿真只用于验证身份隔离、基本启动和导航能力。

### 8.2 电量管理

* 实现 `amr_power_manager`。
* 管理电池配置及电量状态。
* 实现电量变化模拟。
* 支持充电点、充电状态及自动充电任务。

### 8.3 高级任务调度

* 支持多个任务的排队和分配。
* 根据机器人状态选择任务执行对象。
* 考虑任务优先级、机器人可用性和任务执行结果。
* 根据实际需求逐步引入更复杂的调度策略。

### 8.4 管理界面与数据平台

* Web 管理界面。
* 仓库、机器人、地图及任务可视化。
* 历史任务查询及统计分析。
* 根据业务规模引入 MySQL、Redis 等数据组件。

上述规划可随项目实际进展调整。进入开发前，应先明确需求、验收条件和对现有架构的影响。

---

## 9. 待确认事项（TBD）

以下问题尚未完全确定，不视为已经冻结的设计决策。

| 编号      | 问题                 | 需要明确的内容                                                |
| ------- | ------------------ | ------------------------------------------------------ |
| TBD-001 | Cloud 与 Edge 的任务接口 | Cloud 通过 HTTP 还是 MQTT 创建任务，是否需要同时支持两种方式？               |
| TBD-002 | 任务分配方式             | V1 的目标 AMR 由 Cloud 指定，还是由 Edge 根据明确的简单规则选择？            |
| TBD-003 | 地图切换策略             | 下载新地图后，何时允许切换，如何保证运行中的任务不受影响？                          |
| TBD-004 | 任务取消的安全策略          | 取消导航时如何处理机器人当前运动，以及如何确认机器人已经达到可接受的停止状态？                |
| TBD-005 | AMR 身份模型           | `robot_id` 与 `simulation_instance_id` 的职责，以及真实机器人和仿真实例之间的对应关系是什么？ |
| TBD-006 | MQTT 安全            | V1 使用何种最小可行的身份认证与访问控制策略？                               |
| TBD-007 | 导航目标定义             | 任务目标使用地图坐标、站点 ID，还是其他业务标识？                             |
| TBD-008 | MQTT 消息可靠性         | 任务消息采用何种 QoS、消息确认和重发策略？                                |
| TBD-009 | 断网期间的任务取消          | 如果 Cloud 发起取消，但 AMR 正在断网，任务是否必须在恢复通信后取消，还是允许在原任务完成后处理？ |
| TBD-010 | 任务状态冲突             | 如果 Edge 的最后已知状态与 AMR 补报状态不一致，应采用什么规则处理？                |
| TBD-011 | 结果持久化              | AMR 的待补报结果保存到内存、文件还是本地数据库？需要保证多长时间内不丢失？                |
| TBD-012 | 任务终态后的处理           | 已完成或已取消的任务是否允许重新下发？如允许，应如何区分重试与新任务？                    |
| TBD-013 | 仿真与业务状态关联          | 仿真 AMR 的生命周期状态和 AMR 业务状态如何关联？                          |
| TBD-014 | 任务下发/取消 Topic       | 任务下发、任务接受、状态上报和取消请求的具体 Topic、QoS 和幂等字段如何定义？              |
| TBD-015 | 地图包内部结构            | V1 地图包是否固定包含 `map.yaml`、`map.pgm`、`zones.yaml`、`stations.yaml`，是否允许其他布局？ |
| TBD-016 | 地图下载地址              | AMR 从注册响应获得的是相对路径还是完整 URL，Edge 如何生成可访问的下载地址？                |
| TBD-017 | Cloud 最小实现           | V1 的 Cloud Platform 以 CLI、HTTP 脚本还是 FastAPI 服务交付，任务创建参数如何输入？         |
| TBD-018 | 仿真启动与身份分配          | 多 AMR 的 `robot_id`、`simulation_instance_id`、Gazebo 模型名和 ROS2 命名空间由谁分配并保持一致？ |

上述问题应在相关模块开始实现前逐步明确，不要求一次性全部解决。涉及任务取消、断网续行和结果补报的关键问题，应在对应功能实现前确定最低可行规则。

---

## 10. 版本与变更管理

### 10.1 当前版本基线

当前开发目标为 V1。

只有明确标记为 V1 的 Must 需求才是必须完成的核心交付项。

V2 规划项不应自动转化为 V1 的开发任务。

### 10.2 需求变更原则

新增需求时，应先判断：

1. 是否属于 V1 的必要功能。
2. 是否影响既有模块职责。
3. 是否改变系统通信边界。
4. 是否增加额外依赖或明显提高验证成本。
5. 是否需要同步更新架构、设计和验收文档。

若新增需求不属于 V1 的必要范围，默认记录为后续版本规划，而不是立即实现。

### 10.3 V1 完成定义

当以下条件全部满足时，可认为 V1 达到阶段性完成：

* 核心 Must 需求已实现。
* 核心验收标准已通过。
* Cloud 创建任务、Edge 下发、AMR 执行、任务结果反馈的基本闭环能够稳定复现。
* 任务取消流程能够验证，并能正确反馈取消结果。
* MQTT 中断期间当前任务能够在安全条件允许时继续执行。
* 通信恢复后，任务结果能够补报，Edge 与 Cloud 能够获取正确的最终状态。
* 构建、配置和运行步骤有文档记录。
* 关键通信边界与模块职责符合架构约束。
* 不存在阻止核心业务闭环运行的已知问题。

达到上述条件后，应先整理项目成果、测试证据和文档，再决定是否启动 V2，而不是继续无边界地向 V1 添加功能。

### 10.4 文档修订记录

| 版本 | 日期 | 主要变更 |
| --- | --- | --- |
| V0.1 | 初始版本 | 建立 SRS 基本框架。 |
| V0.2 | 2026-10-09 | 对齐当前代码基线；补充支撑模块、假设依赖、关键接口、数据对象、仿真可复现性 NFR、需求追溯矩阵和 TBD。 |
| V0.3 | 2026-10-09 | 按正式命名将 `amr_manager` 统一修正为 `amr_behavior_manager`，并同步代码包名、命名空间、节点名和接口注释。 |

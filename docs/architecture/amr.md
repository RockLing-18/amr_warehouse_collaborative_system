# AMR Internal Architecture

**Status:** Partial

V1 modules are partially implemented. V2 adds a standalone `amr_power_manager` and an **in-package traffic-right logic `TrafficGate` inside `amr_behavior_manager`** (no standalone traffic module).

## 1. Overview

AMR is a ROS2-based autonomous mobile robot system.

The AMR internal modules communicate through ROS2.  
The AMR does not directly communicate with the Cloud Platform.

Communication with the Edge Server is handled by `amr_agent` through

- HTTP
- MQTT

The V1 AMR modules are:

- `amr_agent`
- `amr_behavior_manager`
- `amr_navigation`

V2 additions, not part of V1:

- `amr_power_manager` — standalone ROS2 package (battery / charging state).
- `TrafficGate` — traffic-right classes / BT nodes **inside `amr_behavior_manager`**; there is deliberately **no standalone traffic module or package**.

## 2. Architecture

```mermaid
architecture-beta

    group amr(cloud)[AMR]

        service agent(server)[amr_agent] in amr
        service behavior(server)[amr_behavior_manager] in amr
        service navigation(server)[amr_navigation] in amr
        %% V2: traffic-right logic (TrafficGate) lives INSIDE amr_behavior_manager; no standalone service
        service power(server)[amr_power_manager (V2)] in amr

    group edge(cloud)[Edge Server]

        service edge_http(server)[HTTP Server] in edge
        service edge_mqtt(server)[MQTT Broker] in edge


    agent:R -- L:edge_http
    agent:R -- L:edge_mqtt

    agent:B -- T:behavior

    behavior:B -- T:navigation
    behavior:R -- L:power
```

## 3. Module Responsibilities

### 3.1 amr_agent
amr_agent is the AMR external communication and system integration module.

Responsibilities:

AMR bootstrap
Edge Server connection
HTTP communication with Edge Server
MQTT connection and message handling
AMR registration
AMR status reporting
Map synchronization
Receiving external tasks
Forwarding tasks to the behavior manager
Reporting task and system status to Edge Server

amr_agent does not implement:

Navigation algorithms
Task behavior orchestration
Traffic decision logic
Battery management logic
### 3.2 amr_behavior_manager

amr_behavior_manager is responsible for task execution orchestration.

Responsibilities:

Task lifecycle management
Task execution sequencing
Behavior Tree execution
Failure handling
Recovery behavior
Triggering navigation
V2 traffic-right decision (in-package TrafficGate: trigger / request / wait / release)
Coordinating power management
Determining the next behavior based on the current task state

The behavior manager does not directly implement:

Path planning
Motion control
Global traffic resource arbitration (Edge-side ResourceBroker)
Battery hardware management

Path planning / motion control belong to `amr_navigation`; global resource arbitration belongs to Edge Server; battery state belongs to `amr_power_manager`. Traffic-right **decision** is in-package (3.4).

### 3.3 amr_navigation

amr_navigation provides autonomous navigation capabilities based on Nav2.

Responsibilities:

Navigation initialization
Map loading
Localization
Path planning
Path following
Navigation execution
Navigation result reporting
Robot pose information

The navigation module focuses on:

How the robot moves from the current position to the target position.

It does not decide:

Why the robot should move to that position.

That decision belongs to the behavior/task layer.

### 3.4 V2 traffic-right logic — TrafficGate (inside amr_behavior_manager)

**There is no standalone traffic module.** Project owner confirmed on 2026-11-20 that V2 traffic-right logic is `TrafficGate`: a set of classes / BT nodes **inside the `amr_behavior_manager` ROS2 package**. All documents and code must not refer to a separate standalone traffic module or package (that name is retired).

**Why in-package**: `current_pose` (action feedback), the 4-point `traffic_gates.yaml` geometry loaded by `traffic_gate_loader`, and `NavigationClient::cancelNavigation()` all live in `amr_behavior_manager`. Moving the logic to `amr_agent` would add three cross-process links (pose forwarding, geometry duplication, a new cancel-navigation interface). See `roadmap/v2-traffic-control-chain.md` §3.

> **V0.20 note (2026-11-22)**: the geometry / trigger model was re-decided — independent `traffic_gates.yaml` (4-point control zone + `wait_poses{l1,l2}`), centre-point in/out edges as the primary judgement, **no `plan` subscription**, and `traffic_gates_editor` for data entry. The earlier directed trigger/release segment and `crossed()` model is retired. Authority: `.workbuddy/artifacts/traffic-gate-8-decisions-frozen.md`.

Responsibilities:

- Consume `amr_navigate_to_pose` action feedback `current_pose` only (fill the V1 empty feedback callback). **No `plan` subscription**; the 1 Hz `robot_pose` is never a primary judgement source. Hysteresis (in-shrink on entry / out-expand on exit) damps boundary noise.
- Load `traffic_gates.yaml` via `traffic_gate_loader`: 4-point convex `polygon` (point order `P1→P2` end edge L1 / `P2→P3` long axis / `P3→P4` end edge L2) and `wait_poses{l1,l2}`; **no** `side` / `forward` / `trigger` / `release` fields. The zone is a lengthened traffic-control area, not the physical 3x2 corridor.
- `inside = point_in_poly(center, polygon)` edge detection: entering (`false→true`) → publish `TrafficRequest` on ROS2 `traffic_request` with `op` / `request_id` / `resource_id` / `epoch` / `eta_sec` / `lease_ttl_sec`. Compute `entry_side` once per crossing (`s = dot(enter_pose - centroid, long_axis)`; `s<0` → L1, `s>0` → L2), latch it, and pick the nearest wait pose **from that side's group only**. `crossed()` remains only as an auxiliary side-entry anomaly detector.
- Handle `TrafficResponse` on ROS2 `traffic_response`: `GRANTED` in `REQUESTING` → **zero action** (AC-020); `GRANTED` in `WAITING` → immediately `navigateToPose(D)`; `REJECTED` → cancel navigation, wait for the result callback (`m_goal_active=false`), navigate to the same-side wait point, fixed 10 s retry.
- Release on the earliest of four triggers (leaving the zone / task end / `lease_ttl` expiry / explicit `CANCEL`). A reroute is covered by leaving the zone or by TTL; there is **no separate `onPlanChanged()` mechanism**.
- Call `NavigationClient` **in-process**; never connect to MQTT directly (`amr_agent` remains the only MQTT gateway).

TrafficGate does not determine the overall task sequence, does not hold global lease state (Edge does), and does not change `task_state` (INV-13).

### 3.5 amr_power_manager (V2 / Planned)

amr_power_manager is a future module. It is not part of V1.

amr_power_manager manages AMR power and battery state.

Responsibilities:

Battery configuration
Battery state monitoring
Battery level reporting
Charging state management
Providing power-related state to the behavior manager

The power manager does not determine the overall task sequence.

## 4. Module Collaboration

The AMR follows a layered responsibility model:

External Communication
        │
        ▼
   amr_agent
        │
        ▼
amr_behavior_manager
        │
   ┌────┴────┐
   ▼         ▼
Navigation   Power

The responsibilities are intentionally separated:

- `amr_agent` handles external communication and system integration (V2: `TrafficBridge` transparent MQTT ↔ ROS2 bridge); it does not hold pose, geometry or business decisions.
- `amr_behavior_manager` handles task orchestration, decision flow **and (V2) traffic-right decision** via in-package `TrafficGate`.
- `amr_navigation` handles robot navigation execution.
- `amr_power_manager` handles battery and charging state (V2).
- Edge Server holds the global lease table and FIFO queue; no AMR module sees another AMR's traffic state.

## 5. Communication
### AMR ↔ Edge Server

The AMR communicates with the Edge Server using:

HTTP
MQTT

Only amr_agent directly communicates with the Edge Server.

Other AMR modules should not directly depend on HTTP or MQTT.

amr_agent
    │
    ├── HTTP ──────► Edge HTTP Server
    │
    └── MQTT ──────► Edge MQTT Broker
### AMR Internal Communication

AMR modules communicate through ROS2.

Depending on the interaction, ROS2 communication may use:

Topic
Service
Action

The specific interfaces are defined in amr_interfaces.

## 6. Design Principles
Single Responsibility

Each module has a clearly defined responsibility.

Communication Isolation

Only amr_agent is responsible for external Edge communication.

Business modules should not directly depend on HTTP or MQTT.

Behavior / Execution Separation

The behavior manager determines:

What should happen next?

Navigation determines:

How should the robot move?

TrafficGate (inside behavior manager) determines:

Can the robot currently proceed?

Power management determines:

Does the robot have sufficient power and what is its charging state?

ROS2-based Internal Architecture

AMR internal modules communicate through ROS2 instead of directly calling each other's implementation classes.

This keeps module dependencies explicit and allows each module to evolve independently.

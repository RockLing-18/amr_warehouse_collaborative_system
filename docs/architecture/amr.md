# AMR Internal Architecture

## 1. Overview

AMR is a ROS2-based autonomous mobile robot system.

The AMR internal modules communicate through ROS2.  
The AMR does not directly communicate with the Cloud Platform.

Communication with the Edge Server is handled by `amr_agent` through

- HTTP
- MQTT

The main AMR modules are

- `amr_agent`
- `amr_behavior_manager` 
- `amr_navigation`
- `amr_traffic_manager`
- `amr_power_manager`

## 2. Architecture

```mermaid
architecture-beta

    group amr(cloud)[AMR]

        service agent(server)[amr_agent] in amr
        service behavior(server)[amr_behavior_manager] in amr
        service navigation(server)[amr_navigation] in amr
        service traffic(server)[amr_traffic_manager] in amr
        service power(server)[amr_power_manager] in amr

    group edge(cloud)[Edge Server]

        service edge_http(server)[HTTP Server] in edge
        service edge_mqtt(server)[MQTT Broker] in edge


    agentR -- Ledge_http
    agentR -- Ledge_mqtt

    agentB -- Tbehavior

    behaviorB -- Tnavigation
    behaviorR -- Ltraffic
    behaviorR -- Lpower
	
3. Module Responsibilities
3.1 amr_agent

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
3.2 amr_behavior_manager

amr_behavior_manager is responsible for task execution orchestration.

Responsibilities:

Task lifecycle management
Task execution sequencing
Behavior Tree execution
Failure handling
Recovery behavior
Triggering navigation
Coordinating traffic management
Coordinating power management
Determining the next behavior based on the current task state

The behavior manager does not directly implement:

Path planning
Motion control
Traffic resource management
Battery hardware management

These responsibilities belong to dedicated modules.

3.3 amr_navigation

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

3.4 amr_traffic_manager

amr_traffic_manager manages traffic-related coordination between AMRs.

Responsibilities:

Detecting traffic/resource conflicts
Requesting traffic permissions from Edge Server
Handling traffic responses
Reporting traffic state
Coordinating with the behavior manager when movement must wait or resume

The traffic manager does not determine the overall task sequence.

3.5 amr_power_manager

amr_power_manager manages AMR power and battery state.

Responsibilities:

Battery configuration
Battery state monitoring
Battery level reporting
Charging state management
Providing power-related state to the behavior manager

The power manager does not determine the overall task sequence.

4. Module Collaboration

The AMR follows a layered responsibility model:

External Communication
        │
        ▼
   amr_agent
        │
        ▼
amr_behavior_manager
        │
   ┌────┼────┐
   ▼    ▼    ▼
Navigation Traffic Power

The responsibilities are intentionally separated:

amr_agent handles external communication and system integration.
amr_behavior_manager handles task orchestration and decision flow.
amr_navigation handles robot navigation execution.
amr_traffic_manager handles traffic coordination.
amr_power_manager handles battery and charging state.
5. Communication
AMR ↔ Edge Server

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
AMR Internal Communication

AMR modules communicate through ROS2.

Depending on the interaction, ROS2 communication may use:

Topic
Service
Action

The specific interfaces are defined in amr_interfaces.

6. Design Principles
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

Traffic management determines:

Can the robot currently proceed?

Power management determines:

Does the robot have sufficient power and what is its charging state?

ROS2-based Internal Architecture

AMR internal modules communicate through ROS2 instead of directly calling each other's implementation classes.

This keeps module dependencies explicit and allows each module to evolve independently.
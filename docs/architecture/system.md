# System Architecture

**Status:** Target Architecture

This document shows the target architecture. V1/V2 boundaries are marked in the components.

```mermaid
architecture-beta

    group cloud(cloud)[Cloud Platform (V2)]
    service cloud_api(server)[Cloud API (V2)] in cloud
    service cloud_mqtt(server)[MQTT Client (V2)] in cloud


    group cloud_mqtt_infra(cloud)[Cloud MQTT]
    service cloud_emqx(server)[EMQX] in cloud_mqtt_infra


    group edge(cloud)[Edge Server]
    service edge_http(server)[HTTP Server or Client] in edge
    service edge_mqtt(server)[MQTT Client] in edge
    service edge_ws(server)[WebSocket Server] in edge


    group edge_mqtt_infra(cloud)[Edge MQTT]
    service edge_emqx(server)[EMQX] in edge_mqtt_infra


    group amr(cloud)[AMR]
    service agent(server)[amr_agent] in amr
    service behavior(server)[amr_behavior_manager] in amr
    service navigation(server)[amr_navigation] in amr
    service traffic(server)[amr_traffic_manager (V2)] in amr
    service power(server)[amr_power_manager (V2)] in amr


    group simulation(cloud)[Simulation]
    service simulation_manager(server)[simulation_manager] in simulation
    service gazebo(server)[Gazebo] in simulation


    cloud_api:R -- L:edge_http

    cloud_mqtt:R -- L:cloud_emqx
    cloud_emqx:R -- L:edge_mqtt

    edge_mqtt:R -- L:edge_emqx
    edge_emqx:R -- L:agent

    edge_http:R -- L:agent

    simulation_manager:R -- L:edge_ws

    agent:B -- T:behavior
    behavior:B -- T:navigation
    behavior:R -- L:traffic
    behavior:R -- L:power

    simulation_manager:B -- T:gazebo
```

## 状态说明

- V1：Edge Server、Edge MQTT、AMR（`amr_agent`、`amr_behavior_manager`、`amr_navigation`）、Simulation。
- V2/Planned：Cloud Platform、Cloud MQTT、`amr_traffic_manager`、`amr_power_manager`。

V1 暂不实现 Cloud Platform，任务通过 Edge 侧任务注入完成闭环。

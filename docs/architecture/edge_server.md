# Edge Server Architecture

## 1. Overview

The Edge Server is the local coordination and service layer for a warehouse.

It is implemented as a standalone C++ application and does not depend on ROS2.

The Edge Server provides communication and services for:

- Cloud Platform
- AMR fleet
- Simulation Manager

The Edge Server acts as:

- HTTP Server for Cloud requests
- HTTP Client for Cloud APIs when Edge initiates requests
- MQTT Client for Cloud-side MQTT communication
- MQTT Client for AMR-side MQTT communication
- WebSocket Server for Simulation Manager

The Edge Server also manages local warehouse data and business services.

---

## 2. Architecture

```mermaid
architecture-beta

    group edge(cloud)[Edge Server]

        service http_server(server)[HTTP Server] in edge
        service http_client(server)[HTTP Client] in edge
        service mqtt_client(server)[MQTT Client] in edge
        service websocket(server)[WebSocket Server] in edge

        service bootstrap(server)[Bootstrap Service] in edge
        service map(server)[Map Service] in edge
        service robot(server)[Robot Service] in edge
        service task(server)[Task Service] in edge

        service database(database)[SQLite] in edge


    group cloud(cloud)[Cloud Platform]

        service cloud_api(server)[Cloud API] in cloud
        service cloud_mqtt(server)[Cloud MQTT] in cloud


    group cloud_mqtt_infra(cloud)[Cloud MQTT]

        service cloud_emqx(server)[EMQX Broker] in cloud_mqtt_infra


    group edge_mqtt_infra(cloud)[Edge MQTT]

        service edge_emqx(server)[EMQX Broker] in edge_mqtt_infra


    group simulation(cloud)[Simulation]

        service simulation_manager(server)[Simulation Manager] in simulation


    group amr(cloud)[AMR Fleet]

        service amr_agent(server)[amr_agent] in amr


    cloud_api:R -- L:http_server
    http_client:R -- L:cloud_api

    cloud_mqtt:R -- L:cloud_emqx
    cloud_emqx:R -- L:mqtt_client

    mqtt_client:R -- L:edge_emqx
    edge_emqx:R -- L:amr_agent

    simulation_manager:R -- L:websocket

    http_server:B -- T:bootstrap
    http_server:B -- T:map

    mqtt_client:B -- T:robot
    mqtt_client:B -- T:task

    bootstrap:R -- L:database
    map:R -- L:database
    robot:R -- L:database
    task:R -- L:database
	
3. Communication Architecture

The Edge Server participates in three external communication domains.

3.1 Cloud HTTP

The Edge Server exposes HTTP APIs to the Cloud Platform.

Cloud API
    │
    │ HTTP
    ▼
Edge HTTP Server

Typical responsibilities include:

AMR bootstrap
Map upload
Map download
Active map management
Warehouse-related operations

The Edge Server may also act as an HTTP Client when it needs to actively call Cloud APIs.

Edge HTTP Client
    │
    │ HTTP
    ▼
Cloud API
3.2 Cloud MQTT

Cloud and Edge use a dedicated MQTT infrastructure.

Cloud MQTT Client
        │
        ▼
   Cloud EMQX
        │
        ▼
Edge MQTT Client

The Cloud MQTT broker is independent from the MQTT broker used by AMRs.

The Edge Server connects to the Cloud EMQX as an MQTT Client.

3.3 AMR MQTT

The Edge Server communicates with AMRs through a separate MQTT infrastructure.

Edge MQTT Client
        │
        ▼
   Edge EMQX
        │
        ├────► AMR 01
        ├────► AMR 02
        └────► AMR N

The Edge Server is responsible for:

AMR registration
AMR status communication
Task delivery
Task status reception
AMR-related event handling

AMRs do not communicate with the Cloud MQTT broker directly.

3.4 Simulation WebSocket

The Simulation Manager connects to the Edge Server through WebSocket.

Simulation Manager
        │
        │ WebSocket
        ▼
Edge WebSocket Server

This channel is primarily used for simulation-related robot synchronization and lifecycle information.

Simulation Manager communicates directly with Gazebo and does not use MQTT for simulation control.

4. Internal Architecture

The Edge Server contains several business services.

                         Edge Server
                              │
             ┌────────────────┼────────────────┐
             │                │                │
       Communication      Business          Storage
             │                │                │
      ┌──────┼──────┐    ┌────┼────┐           │
      │      │      │    │    │    │           │
     HTTP   MQTT   WS   Bootstrap Map Robot   SQLite
                               │      │    │
                               └──────┴────┘
                                      │
                                   Task Service

Communication components are responsible for protocol handling.

Business services are responsible for domain logic.

The database layer is responsible for local persistence.

5. Communication Components
5.1 HTTP Server

The HTTP Server exposes REST-style APIs for external clients.

Responsibilities:

Request routing
Request validation
HTTP response generation
Calling business services
File upload/download handling

The HTTP Server should not contain business logic.

Typical flow:

HTTP Request
     │
     ▼
HTTP Server
     │
     ▼
Business Service
     │
     ▼
SQLite / File System
5.2 HTTP Client

The HTTP Client is used when Edge needs to actively communicate with Cloud services.

It should encapsulate:

Connection management
HTTP request construction
Response parsing
Timeout handling
Error handling

Business services should not directly depend on the HTTP library.

5.3 MQTT Client

The MQTT communication layer handles MQTT connections and messages.

Responsibilities:

MQTT connection
Subscription management
Publishing
Receiving messages
Message routing
Connection recovery

Protocol-specific logic should remain inside the communication layer.

Business services should process decoded domain objects rather than raw MQTT messages.

5.4 WebSocket Server

The WebSocket Server provides the communication channel for Simulation Manager.

Responsibilities:

Connection management
Message receiving
Message sending
Client lifecycle management
Message routing

Simulation-specific business logic should remain outside the WebSocket transport layer.

6. Business Services
6.1 Bootstrap Service

The Bootstrap Service handles AMR bootstrap requests.

Responsibilities:

Validate robot identity
Determine warehouse information
Provide Edge MQTT configuration
Return AMR bootstrap information

Typical flow:

AMR
 │
 │ HTTP
 ▼
Bootstrap Service
 │
 ├── Warehouse information
 ├── MQTT configuration
 └── Bootstrap response
6.2 Map Service

The Map Service manages warehouse map packages.

Responsibilities:

Map upload
Map download
Map version management
Active map management
Map activation
Map metadata persistence

Map files are stored on the Edge file system.

Map metadata is stored in SQLite.

6.3 Robot Service

The Robot Service manages AMR-related information.

Responsibilities:

Robot registration
Robot status
Robot lifecycle state
Robot information persistence
Robot communication state
6.4 Task Service

The Task Service manages task-related business data.

Responsibilities:

Task creation
Task assignment
Task state management
Task status updates
Task completion/failure handling

The Task Service does not directly control AMR navigation.

The AMR is responsible for executing the task locally.

7. Data Storage

The Edge Server uses SQLite for local persistent data.

Typical data includes:

Warehouse information
Robot information
Task information
Map metadata
Active map information

Large map packages and other files are stored in the file system rather than directly inside SQLite.

Edge Server
    │
    ├── SQLite
    │     ├── Robot data
    │     ├── Task data
    │     └── Map metadata
    │
    └── File System
          └── Map packages
8. Responsibility Boundaries

The Edge Server follows a layered responsibility model.

Communication Layer

Responsible for:

HTTP
MQTT
WebSocket
Connection management
Message parsing
Message routing
Business Layer

Responsible for:

Bootstrap
Robot management
Task management
Map management
Persistence Layer

Responsible for:

SQLite
File system
Local data persistence

The layers should remain loosely coupled.

For example:

MQTT Message
     │
     ▼
MQTT Router
     │
     ▼
Robot Service
     │
     ▼
SQLite

The Robot Service should not need to know whether the request originated from MQTT, HTTP, or WebSocket.

9. Design Principles
Protocol and Business Logic Separation

Communication components handle protocols.

Business services handle domain logic.

Edge Autonomy

The Edge Server should be capable of providing local warehouse services without requiring every AMR operation to directly access the Cloud Platform.

AMR Communication Isolation

Only the Edge communication layer communicates with AMRs through MQTT.

AMR-specific MQTT topics and message formats should not leak into unrelated business services.

Cloud Communication Isolation

Cloud HTTP/MQTT communication should be isolated from business logic through dedicated communication components.

Local Persistence

Operational data required by the warehouse should be persisted locally.

No ROS2 Dependency

The Edge Server is independent of ROS2.

ROS2 is used only inside the AMR system.
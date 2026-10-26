#include "database/sqlite_db.h"
#include <iostream>
#include <sstream>
#include "utils/LogDefine.h"
#include "utils/CommonFunc.h"
#include "sqlite/sqlite3.h"
#include "nlohmann/json.hpp"

namespace edge_server
{

SQLiteDB& SQLiteDB::Instance()
{
    static SQLiteDB instance;
    return instance;
}

SQLiteDB::SQLiteDB() : m_pDB(nullptr)
{
}

SQLiteDB::~SQLiteDB()
{
    if(m_pDB)
        sqlite3_close(m_pDB);
}

bool SQLiteDB::Open(const std::string& dbPath)
{
    std::lock_guard<std::mutex> lock(m_Mu);

    if(sqlite3_open(dbPath.c_str(), &m_pDB) != SQLITE_OK)
    {
        LOG_ERROR("Open DB failed:{} ", sqlite3_errmsg(m_pDB));
        return false;
    }

    LOG_INFO("path:{}", dbPath);

    // 开启 WAL 模式
    char* errMsg = nullptr;
    if(sqlite3_exec(m_pDB, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &errMsg) != SQLITE_OK)
    {
        LOG_ERROR("Enable WAL failed:{} ",errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

bool SQLiteDB::InitTables()
{
    std::lock_guard<std::mutex> lock(m_Mu);

    if(!m_pDB)
    {
        LOG_ERROR("Database not opened");
        return false;
    }

    if(!InitMapTables())
    {
        return false;
    }

    if(!InitTaskTables())
    {
        return false;
    }

    // 新增：强制WAL全量刷盘，表结构写入主db文件
    char* errCheck = nullptr;
    sqlite3_exec(m_pDB, "PRAGMA wal_checkpoint(FULL);", nullptr, nullptr, &errCheck);
    if(errCheck)
    {
        LOG_WARN("WAL checkpoint warn:{}", errCheck);
        sqlite3_free(errCheck);
    }

    LOG_INFO("SQLite schema initialized successfully");
    return true;
}

bool SQLiteDB::InitMapTables()
{
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS tbl_map_package
        (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            warehouse_id TEXT NOT NULL,
            version TEXT NOT NULL,
            package_name TEXT NOT NULL,
            package_path TEXT NOT NULL,
            package_size INTEGER DEFAULT 0,
            upload_time TEXT DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(warehouse_id, version)
        );

        CREATE TABLE IF NOT EXISTS tbl_active_map
        (
            warehouse_id TEXT PRIMARY KEY,
            package_id INTEGER NOT NULL,
            version TEXT NOT NULL,
            update_time TEXT DEFAULT CURRENT_TIMESTAMP
        );
    )";

    char* err = nullptr;
    if(sqlite3_exec(m_pDB, sql, nullptr, nullptr, &err) != SQLITE_OK)
    {
        LOG_ERROR("init tables failed:{}", err);
        sqlite3_free(err);
        return false;
    }

    return true;
}

bool SQLiteDB::InitTaskTables()
{
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS tbl_robot
        (
            robot_id            TEXT PRIMARY KEY,
            warehouse_id        TEXT NOT NULL,
            robot_name          TEXT,
            register_timestamp  TEXT NOT NULL,
            updated_at          TEXT NOT NULL,
            initial_pose        TEXT
        );

        CREATE TABLE IF NOT EXISTS tbl_robot_instance
        (
            id               INTEGER PRIMARY KEY AUTOINCREMENT,
            robot_id         TEXT NOT NULL,
            instance_id      TEXT NOT NULL,
            model_name       TEXT,
            lifecycle_state  TEXT NOT NULL,
            created_at       TEXT NOT NULL,
            updated_at       TEXT NOT NULL,
            UNIQUE(robot_id, instance_id)
        );

        CREATE INDEX IF NOT EXISTS idx_robot_instance_robot
            ON tbl_robot_instance(robot_id);

        CREATE TABLE IF NOT EXISTS tbl_robot_status
        (
            robot_id        TEXT PRIMARY KEY,
            state           TEXT NOT NULL,
            online          INTEGER NOT NULL DEFAULT 0,
            battery         REAL,
            pose            TEXT,
            current_task_id TEXT,
            updated_at      TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS tbl_task
        (
            task_id      TEXT PRIMARY KEY,
            warehouse_id TEXT NOT NULL,
            task_type    TEXT NOT NULL CHECK(task_type IN ('LOAD', 'UNLOAD')),
            state        TEXT NOT NULL,
            robot_ids    TEXT NOT NULL,
            goal         TEXT,
            reason       TEXT,
            created_at   TEXT NOT NULL,
            updated_at   TEXT NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_task_warehouse_state
            ON tbl_task(warehouse_id, state);

        CREATE TABLE IF NOT EXISTS tbl_task_goods
        (
            id             INTEGER PRIMARY KEY AUTOINCREMENT,
            task_id        TEXT NOT NULL,
            goods_id       TEXT NOT NULL,
            goods_name     TEXT,
            total_quantity INTEGER NOT NULL CHECK(total_quantity >= 0),
            UNIQUE(task_id, goods_id)
        );

        CREATE INDEX IF NOT EXISTS idx_task_goods_task
            ON tbl_task_goods(task_id);

        CREATE TABLE IF NOT EXISTS tbl_task_goods_status
        (
            id                   INTEGER PRIMARY KEY AUTOINCREMENT,
            task_id              TEXT NOT NULL,
            goods_id             TEXT NOT NULL,
            total_quantity       INTEGER NOT NULL CHECK(total_quantity >= 0),
            in_progress_quantity INTEGER NOT NULL DEFAULT 0 CHECK(in_progress_quantity >= 0),
            completed_quantity   INTEGER NOT NULL DEFAULT 0 CHECK(completed_quantity >= 0),
            pending_quantity     INTEGER NOT NULL DEFAULT 0 CHECK(pending_quantity >= 0),
            updated_at           TEXT NOT NULL,
            UNIQUE(task_id, goods_id),
            CHECK(total_quantity = in_progress_quantity + completed_quantity + pending_quantity)
        );

        CREATE INDEX IF NOT EXISTS idx_task_goods_status_task
            ON tbl_task_goods_status(task_id);

        CREATE TABLE IF NOT EXISTS tbl_task_goods_allocation
        (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            request_id    TEXT NOT NULL,
            task_id       TEXT NOT NULL,
            goods_id      TEXT NOT NULL,
            robot_id      TEXT NOT NULL,
            quantity      INTEGER NOT NULL CHECK(quantity > 0),
            state         TEXT NOT NULL DEFAULT 'IN_PROGRESS'
                          CHECK(state IN ('IN_PROGRESS', 'COMPLETED', 'ABORTED')),
            allocated_at  TEXT NOT NULL,
            settled_at    TEXT,
            UNIQUE(request_id, robot_id)
        );

        CREATE INDEX IF NOT EXISTS idx_task_goods_allocation_active
            ON tbl_task_goods_allocation(task_id, goods_id, state);

        CREATE INDEX IF NOT EXISTS idx_task_goods_allocation_robot
            ON tbl_task_goods_allocation(robot_id);

        CREATE TABLE IF NOT EXISTS tbl_task_event
        (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            task_id    TEXT NOT NULL,
            state      TEXT NOT NULL,
            reason     TEXT,
            source     TEXT NOT NULL,
            timestamp  TEXT NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_task_event_task_time
            ON tbl_task_event(task_id, timestamp);

        CREATE TABLE IF NOT EXISTS tbl_amr_task_status_event
        (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            message_id   TEXT NOT NULL,
            task_id      TEXT NOT NULL,
            robot_id     TEXT NOT NULL,
            event_type   TEXT NOT NULL
                         CHECK(event_type IN ('STATUS', 'CARRY_COMPLETED', 'CARRY_ABORTED')),
            task_state   TEXT NOT NULL,
            goods_json   TEXT,
            reason       TEXT,
            timestamp    TEXT NOT NULL,
            received_at  TEXT NOT NULL,
            UNIQUE(message_id, task_id, robot_id, event_type)
        );

        CREATE INDEX IF NOT EXISTS idx_amr_task_status_event_task_time
            ON tbl_amr_task_status_event(task_id, timestamp);
    )";

    char* err = nullptr;
    if(sqlite3_exec(m_pDB, sql, nullptr, nullptr, &err) != SQLITE_OK)
    {
        LOG_ERROR("init task tables failed:{}", err);
        sqlite3_free(err);
        return false;
    }

    // 迁移：旧库 tbl_robot 可能没有 initial_pose 列
    {
        sqlite3_stmt* stmt = nullptr;
        bool hasInitialPose = false;
        if(sqlite3_prepare_v2(m_pDB, "PRAGMA table_info(tbl_robot);", -1, &stmt, nullptr) == SQLITE_OK)
        {
            while(sqlite3_step(stmt) == SQLITE_ROW)
            {
                const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                if(name && std::string(name) == "initial_pose")
                {
                    hasInitialPose = true;
                    break;
                }
            }
            sqlite3_finalize(stmt);
        }

        if(!hasInitialPose)
        {
            char* alterErr = nullptr;
            if(sqlite3_exec(m_pDB, "ALTER TABLE tbl_robot ADD COLUMN initial_pose TEXT;", nullptr, nullptr, &alterErr) != SQLITE_OK)
            {
                LOG_WARN("add tbl_robot.initial_pose failed:{}", alterErr ? alterErr : "");
                if(alterErr) sqlite3_free(alterErr);
            }
        }
    }

    return true;
}

bool SQLiteDB::UploadMapPackage(const MapPackage& package)
{
    std::lock_guard<std::mutex> lock(m_Mu);
    if(!m_pDB)
    {
        LOG_ERROR("database not opened");
        return false;
    }

    BeginTransaction();
    int64_t package_id = 0;

    // 1.插入地图包
    if(!InsertMapPackageInternal(package, package_id))
    {
        Rollback();
        return false;
    }


    // 2.根据 activate 决定是否启用
    if(package.activate)
    {
        if(!SetActiveMapInternal(package.warehouse_id, package_id, package.version))
        {
            Rollback();
            return false;
        }
    }

    Commit();

    LOG_INFO(
        "upload map success warehouse:{} version:{} id:{} activate:{}",
        package.warehouse_id,
        package.version,
        package_id,
        package.activate);

    return true;
}

bool SQLiteDB::GetMapPackage(const std::string& warehouse_id, const std::string& version, MapPackage& package)
{
    std::lock_guard<std::mutex> lock(m_Mu);
    const char* sql=R"(
        SELECT
            id,
            warehouse_id,
            version,
            package_name,
            package_path,
            package_size,
            upload_time
        FROM tbl_map_package 
        WHERE warehouse_id=? and version=?;
    )";

    sqlite3_stmt* stmt=nullptr;
    if(sqlite3_prepare_v2(m_pDB, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    
    sqlite3_bind_text(stmt, 1, warehouse_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, version.c_str(), -1, SQLITE_TRANSIENT);
    bool result = false;
    if(sqlite3_step(stmt)==SQLITE_ROW)
    {
        package.id = sqlite3_column_int64(stmt,0);
        package.warehouse_id = (const char*)sqlite3_column_text(stmt, 1);
        package.version = (const char*)sqlite3_column_text(stmt, 2);
        package.package_name = (const char*)sqlite3_column_text(stmt,3);
        package.package_path = (const char*)sqlite3_column_text(stmt,4);
        package.package_size = sqlite3_column_int64(stmt,5);
        package.upload_time = (const char*)sqlite3_column_text(stmt,6);
        result = true;
    }

    sqlite3_finalize(stmt);
    return result;
}

bool SQLiteDB::InsertMapPackageInternal(const MapPackage& package, int64_t& package_id)
{
    const char* sql=R"(
        INSERT INTO tbl_map_package
        (
            warehouse_id,
            version,
            package_name,
            package_path,
            package_size
        )

        VALUES(?,?,?,?,?);
    )";

    sqlite3_stmt* stmt=nullptr;

    if(sqlite3_prepare_v2(m_pDB, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        LOG_ERROR("prepare insert map failed:{}", sqlite3_errmsg(m_pDB));
        return false;
    }

    sqlite3_bind_text(stmt, 1, package.warehouse_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, package.version.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, package.package_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, package.package_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, package.package_size);

    if(sqlite3_step(stmt) != SQLITE_DONE)
    {
        LOG_ERROR("insert map failed:{}",  sqlite3_errmsg(m_pDB));

        sqlite3_finalize(stmt);
        return false;
    }

    package_id = sqlite3_last_insert_rowid(m_pDB);

    sqlite3_finalize(stmt);
    return true;
}

bool SQLiteDB::SetActiveMapInternal(const std::string& warehouse_id, int64_t package_id, const std::string& version)
{
    const char* sql=R"(
        INSERT INTO tbl_active_map
        (
            warehouse_id,
            package_id,
            version
        )

        VALUES(?,?,?)
        ON CONFLICT(warehouse_id)
        DO UPDATE SET
            package_id=excluded.package_id,
            version=excluded.version,
            update_time=CURRENT_TIMESTAMP;
    )";

    sqlite3_stmt* stmt = nullptr;
    if(sqlite3_prepare_v2(m_pDB, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        LOG_ERROR("prepare active map failed:{}", sqlite3_errmsg(m_pDB));
        return false;
    }

    sqlite3_bind_text(stmt, 1, warehouse_id.c_str(), -1,  SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, package_id);
    sqlite3_bind_text(stmt, 3, version.c_str(), -1, SQLITE_TRANSIENT);
    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    if(!result)
    {
        LOG_ERROR( "set active map failed:{}", sqlite3_errmsg(m_pDB));
    }

    sqlite3_finalize(stmt);
    return result;
}

void SQLiteDB::BeginTransaction()
{
    sqlite3_exec(
        m_pDB,
        "BEGIN TRANSACTION;",
        nullptr,
        nullptr,
        nullptr);
}

void SQLiteDB::Commit()
{
    sqlite3_exec(
        m_pDB,
        "COMMIT;",
        nullptr,
        nullptr,
        nullptr);

    sqlite3_exec(
        m_pDB,
        "PRAGMA wal_checkpoint(PASSIVE);",
        nullptr,
        nullptr,
        nullptr);
}

void SQLiteDB::Rollback()
{
    sqlite3_exec(
        m_pDB,
        "ROLLBACK;",
        nullptr,
        nullptr,
        nullptr);
}

bool SQLiteDB::GetActiveMap(const std::string& warehouse_id, MapPackage& package)
{
    std::lock_guard<std::mutex> lock(m_Mu);
    const char* sql=R"(
        SELECT
            m.id,
            m.warehouse_id,
            m.version,
            m.package_name,
            m.package_path,
            m.package_size,
            m.upload_time
        FROM tbl_active_map a
        JOIN tbl_map_package m
        ON a.package_id=m.id
        WHERE a.warehouse_id=?;
    )";

    sqlite3_stmt* stmt=nullptr;
    if(sqlite3_prepare_v2(m_pDB, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    
    sqlite3_bind_text(stmt, 1, warehouse_id.c_str(), -1, SQLITE_TRANSIENT);
    bool result=false;
    if(sqlite3_step(stmt)==SQLITE_ROW)
    {
        package.id = sqlite3_column_int64(stmt,0);
        package.warehouse_id = (const char*)sqlite3_column_text(stmt, 1);
        package.version = (const char*)sqlite3_column_text(stmt, 2);
        package.package_name = (const char*)sqlite3_column_text(stmt,3);
        package.package_path = (const char*)sqlite3_column_text(stmt,4);
        package.package_size = sqlite3_column_int64(stmt,5);
        package.upload_time = (const char*)sqlite3_column_text(stmt,6);
        result = true;
    }

    sqlite3_finalize(stmt);
    return result;
}

bool SQLiteDB::SetActiveMap(const std::string& warehouse_id, int64_t package_id, const std::string& version)
{
    std::lock_guard<std::mutex> lock(m_Mu);
    const char* sql=R"(
        INSERT INTO tbl_active_map
        (
            warehouse_id,
            package_id,
            version
        )

        VALUES(?,?,?)
        ON CONFLICT(warehouse_id)
        DO UPDATE SET
            package_id=excluded.package_id,
            version=excluded.version,
            update_time=CURRENT_TIMESTAMP;
    )";

    sqlite3_stmt* stmt = nullptr;
    if(sqlite3_prepare_v2(m_pDB, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        LOG_ERROR("prepare active map failed:{}", sqlite3_errmsg(m_pDB));
        return false;
    }

    sqlite3_bind_text(stmt, 1, warehouse_id.c_str(), -1,  SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, package_id);
    sqlite3_bind_text(stmt, 3, version.c_str(), -1, SQLITE_TRANSIENT);
    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    if(!result)
    {
        LOG_ERROR( "set active map failed:{}", sqlite3_errmsg(m_pDB));
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(
        m_pDB,
        "PRAGMA wal_checkpoint(PASSIVE);",
        nullptr,
        nullptr,
        nullptr);
    return result;
}

bool SQLiteDB::UpsertRobot(const RobotBaseInfo& robot, const RobotRunningStatus& status)
{
    std::lock_guard<std::mutex> lock(m_Mu);
    if(!m_pDB)
    {
        LOG_ERROR("database not opened");
        return false;
    }

    std::string now = std::to_string(utils::getCurrentTimeMs());
    std::string registerTimestamp = std::to_string(robot.register_timestamp);

    BeginTransaction();

    const char* robotSql = R"(
        INSERT INTO tbl_robot
        (
            robot_id,
            warehouse_id,
            robot_name,
            register_timestamp,
            updated_at,
            initial_pose
        )
        VALUES(?,?,?,?,?,?)
        ON CONFLICT(robot_id) DO UPDATE SET
            warehouse_id=excluded.warehouse_id,
            robot_name=excluded.robot_name,
            register_timestamp=excluded.register_timestamp,
            updated_at=excluded.updated_at,
            initial_pose=excluded.initial_pose;
    )";

    sqlite3_stmt* stmt = nullptr;
    if(sqlite3_prepare_v2(m_pDB, robotSql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        LOG_ERROR("prepare upsert robot failed:{}", sqlite3_errmsg(m_pDB));
        Rollback();
        return false;
    }

    nlohmann::json initialPoseJson;
    initialPoseJson["x"] = robot.initial_pose.x;
    initialPoseJson["y"] = robot.initial_pose.y;
    initialPoseJson["yaw"] = robot.initial_pose.yaw;

    sqlite3_bind_text(stmt, 1, robot.robot_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, robot.warehouse_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, registerTimestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, initialPoseJson.dump().c_str(), -1, SQLITE_TRANSIENT);

    if(sqlite3_step(stmt) != SQLITE_DONE)
    {
        LOG_ERROR("upsert robot failed:{}", sqlite3_errmsg(m_pDB));
        sqlite3_finalize(stmt);
        Rollback();
        return false;
    }
    sqlite3_finalize(stmt);

    if(!robot.instance_id.empty())
    {
        std::string modelName = "amr_" + robot.robot_id + "_" + robot.instance_id;

        const char* instanceSql = R"(
            INSERT INTO tbl_robot_instance
            (
                robot_id,
                instance_id,
                model_name,
                lifecycle_state,
                created_at,
                updated_at
            )
            VALUES(?,?,?,?,?,?)
            ON CONFLICT(robot_id, instance_id) DO UPDATE SET
                model_name=excluded.model_name,
                lifecycle_state=excluded.lifecycle_state,
                updated_at=excluded.updated_at;
        )";

        sqlite3_stmt* instanceStmt = nullptr;
        if(sqlite3_prepare_v2(m_pDB, instanceSql, -1, &instanceStmt, nullptr) != SQLITE_OK)
        {
            LOG_ERROR("prepare upsert robot instance failed:{}", sqlite3_errmsg(m_pDB));
            Rollback();
            return false;
        }

        sqlite3_bind_text(instanceStmt, 1, robot.robot_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(instanceStmt, 2, robot.instance_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(instanceStmt, 3, modelName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(instanceStmt, 4, "REGISTERED", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(instanceStmt, 5, registerTimestamp.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(instanceStmt, 6, now.c_str(), -1, SQLITE_TRANSIENT);

        if(sqlite3_step(instanceStmt) != SQLITE_DONE)
        {
            LOG_ERROR("upsert robot instance failed:{}", sqlite3_errmsg(m_pDB));
            sqlite3_finalize(instanceStmt);
            Rollback();
            return false;
        }
        sqlite3_finalize(instanceStmt);
    }

    nlohmann::json poseJson;
    poseJson["x"] = status.pose.x;
    poseJson["y"] = status.pose.y;
    poseJson["yaw"] = status.pose.yaw;

    const char* statusSql = R"(
        INSERT INTO tbl_robot_status
        (
            robot_id,
            state,
            online,
            battery,
            pose,
            current_task_id,
            updated_at
        )
        VALUES(?,?,?,?,?,?,?)
        ON CONFLICT(robot_id) DO UPDATE SET
            state=excluded.state,
            online=excluded.online,
            battery=excluded.battery,
            pose=excluded.pose,
            current_task_id=excluded.current_task_id,
            updated_at=excluded.updated_at;
    )";

    sqlite3_stmt* statusStmt = nullptr;
    if(sqlite3_prepare_v2(m_pDB, statusSql, -1, &statusStmt, nullptr) != SQLITE_OK)
    {
        LOG_ERROR("prepare upsert robot status failed:{}", sqlite3_errmsg(m_pDB));
        Rollback();
        return false;
    }

    sqlite3_bind_text(statusStmt, 1, robot.robot_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statusStmt, 2, RobotStateToString(status.state).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statusStmt, 3, status.online ? 1 : 0);
    sqlite3_bind_double(statusStmt, 4, status.battery);
    sqlite3_bind_text(statusStmt, 5, poseJson.dump().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statusStmt, 6, status.task_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statusStmt, 7, now.c_str(), -1, SQLITE_TRANSIENT);

    if(sqlite3_step(statusStmt) != SQLITE_DONE)
    {
        LOG_ERROR("upsert robot status failed:{}", sqlite3_errmsg(m_pDB));
        sqlite3_finalize(statusStmt);
        Rollback();
        return false;
    }
    sqlite3_finalize(statusStmt);

    Commit();
    return true;
}

bool SQLiteDB::LoadRobots(std::vector<RobotBaseInfo>& robots, std::vector<RobotRunningStatus>& statuses)
{
    std::lock_guard<std::mutex> lock(m_Mu);
    if(!m_pDB)
    {
        LOG_ERROR("database not opened");
        return false;
    }

    robots.clear();
    statuses.clear();

    const char* sql = R"(
        SELECT
            r.robot_id,
            r.warehouse_id,
            r.register_timestamp,
            (SELECT i.instance_id FROM tbl_robot_instance i
              WHERE i.robot_id = r.robot_id ORDER BY i.id DESC LIMIT 1) AS instance_id,
            s.state,
            s.online,
            s.battery,
            s.pose,
            s.current_task_id,
            r.initial_pose
        FROM tbl_robot r
        LEFT JOIN tbl_robot_status s ON s.robot_id = r.robot_id;
    )";

    sqlite3_stmt* stmt = nullptr;
    if(sqlite3_prepare_v2(m_pDB, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        LOG_ERROR("prepare load robots failed:{}", sqlite3_errmsg(m_pDB));
        return false;
    }

    while(sqlite3_step(stmt) == SQLITE_ROW)
    {
        RobotBaseInfo robot;
        robot.robot_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        robot.warehouse_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

        const char* regTs = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if(regTs)
        {
            try { robot.register_timestamp = std::stoull(regTs); }
            catch(...) { robot.register_timestamp = 0; }
        }

        const char* instanceId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        if(instanceId)
            robot.instance_id = instanceId;

        RobotRunningStatus status;
        status.robot_id = robot.robot_id;
        status.state = RobotState::OFFLINE;
        status.online = false;
        status.battery = sqlite3_column_double(stmt, 6);

        const char* pose = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        if(pose)
        {
            try
            {
                auto poseJson = nlohmann::json::parse(pose);
                status.pose.x = poseJson.value("x", 0.0);
                status.pose.y = poseJson.value("y", 0.0);
                status.pose.yaw = poseJson.value("yaw", 0.0);
            }
            catch(...)
            {
            }
        }

        const char* taskId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        if(taskId)
            status.task_id = taskId;

        const char* initialPose = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        if(initialPose)
        {
            try
            {
                auto initialPoseJson = nlohmann::json::parse(initialPose);
                robot.initial_pose.x = initialPoseJson.value("x", 0.0);
                robot.initial_pose.y = initialPoseJson.value("y", 0.0);
                robot.initial_pose.yaw = initialPoseJson.value("yaw", 0.0);
            }
            catch(...)
            {
            }
        }

        robots.push_back(robot);
        statuses.push_back(status);
    }

    sqlite3_finalize(stmt);
    return true;
}

bool SQLiteDB::CreateTask(const TransportTask& task)
{
    std::lock_guard<std::mutex> lock(m_Mu);
    if(!m_pDB)
    {
        LOG_ERROR("database not opened");
        return false;
    }

    nlohmann::json robotIds = task.robot_ids;

    BeginTransaction();

    const char* insertTaskSql = R"(
        INSERT INTO tbl_task
        (
            task_id,
            warehouse_id,
            task_type,
            state,
            robot_ids,
            goal,
            reason,
            created_at,
            updated_at
        )
        VALUES(?,?,?,?,?,?,?,?,?);
    )";

    sqlite3_stmt* stmt = nullptr;
    if(sqlite3_prepare_v2(m_pDB, insertTaskSql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        LOG_ERROR("prepare create task failed:{}", sqlite3_errmsg(m_pDB));
        Rollback();
        return false;
    }

    sqlite3_bind_text(stmt, 1, task.task_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, task.warehouse_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, task.task_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, task.state.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, robotIds.dump().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, task.goal.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, task.reason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, task.created_at.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, task.updated_at.c_str(), -1, SQLITE_TRANSIENT);

    if(sqlite3_step(stmt) != SQLITE_DONE)
    {
        LOG_ERROR("insert task failed:{}", sqlite3_errmsg(m_pDB));
        sqlite3_finalize(stmt);
        Rollback();
        return false;
    }
    sqlite3_finalize(stmt);

    const char* insertGoodsSql = R"(
        INSERT INTO tbl_task_goods
        (
            task_id,
            goods_id,
            goods_name,
            total_quantity
        )
        VALUES(?,?,?,?);
    )";

    const char* insertGoodsStatusSql = R"(
        INSERT INTO tbl_task_goods_status
        (
            task_id,
            goods_id,
            total_quantity,
            in_progress_quantity,
            completed_quantity,
            pending_quantity,
            updated_at
        )
        VALUES(?,?,?,0,0,?,?);
    )";

    for(const auto& goods : task.goods)
    {
        sqlite3_stmt* goodsStmt = nullptr;
        if(sqlite3_prepare_v2(m_pDB, insertGoodsSql, -1, &goodsStmt, nullptr) != SQLITE_OK)
        {
            LOG_ERROR("prepare insert task goods failed:{}", sqlite3_errmsg(m_pDB));
            Rollback();
            return false;
        }

        sqlite3_bind_text(goodsStmt, 1, task.task_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(goodsStmt, 2, goods.goods_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(goodsStmt, 3, goods.goods_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(goodsStmt, 4, goods.total_quantity);

        if(sqlite3_step(goodsStmt) != SQLITE_DONE)
        {
            LOG_ERROR("insert task goods failed:{}", sqlite3_errmsg(m_pDB));
            sqlite3_finalize(goodsStmt);
            Rollback();
            return false;
        }
        sqlite3_finalize(goodsStmt);

        sqlite3_stmt* statusStmt = nullptr;
        if(sqlite3_prepare_v2(m_pDB, insertGoodsStatusSql, -1, &statusStmt, nullptr) != SQLITE_OK)
        {
            LOG_ERROR("prepare insert task goods status failed:{}", sqlite3_errmsg(m_pDB));
            Rollback();
            return false;
        }

        sqlite3_bind_text(statusStmt, 1, task.task_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statusStmt, 2, goods.goods_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(statusStmt, 3, goods.total_quantity);
        sqlite3_bind_int(statusStmt, 4, goods.total_quantity);
        sqlite3_bind_text(statusStmt, 5, task.updated_at.c_str(), -1, SQLITE_TRANSIENT);

        if(sqlite3_step(statusStmt) != SQLITE_DONE)
        {
            LOG_ERROR("insert task goods status failed:{}", sqlite3_errmsg(m_pDB));
            sqlite3_finalize(statusStmt);
            Rollback();
            return false;
        }
        sqlite3_finalize(statusStmt);
    }

    const char* insertEventSql = R"(
        INSERT INTO tbl_task_event
        (
            task_id,
            state,
            reason,
            source,
            timestamp
        )
        VALUES(?,?,?,'HTTP',?);
    )";

    sqlite3_stmt* eventStmt = nullptr;
    if(sqlite3_prepare_v2(m_pDB, insertEventSql, -1, &eventStmt, nullptr) != SQLITE_OK)
    {
        LOG_ERROR("prepare insert task event failed:{}", sqlite3_errmsg(m_pDB));
        Rollback();
        return false;
    }

    sqlite3_bind_text(eventStmt, 1, task.task_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(eventStmt, 2, task.state.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(eventStmt, 3, task.reason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(eventStmt, 4, task.updated_at.c_str(), -1, SQLITE_TRANSIENT);

    if(sqlite3_step(eventStmt) != SQLITE_DONE)
    {
        LOG_ERROR("insert task event failed:{}", sqlite3_errmsg(m_pDB));
        sqlite3_finalize(eventStmt);
        Rollback();
        return false;
    }
    sqlite3_finalize(eventStmt);

    Commit();
    return true;
}

bool SQLiteDB::TaskExists(const std::string& task_id)
{
    std::lock_guard<std::mutex> lock(m_Mu);
    if(!m_pDB)
        return false;

    const char* sql = "SELECT 1 FROM tbl_task WHERE task_id=?;";
    sqlite3_stmt* stmt = nullptr;
    if(sqlite3_prepare_v2(m_pDB, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text(stmt, 1, task_id.c_str(), -1, SQLITE_TRANSIENT);
    bool exists = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return exists;
}

bool SQLiteDB::GetTask(const std::string& task_id, TransportTask& task)
{
    std::lock_guard<std::mutex> lock(m_Mu);
    if(!m_pDB)
        return false;

    const char* sql = R"(
        SELECT
            task_id,
            warehouse_id,
            task_type,
            state,
            robot_ids,
            goal,
            reason,
            created_at,
            updated_at
        FROM tbl_task
        WHERE task_id=?;
    )";

    sqlite3_stmt* stmt = nullptr;
    if(sqlite3_prepare_v2(m_pDB, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text(stmt, 1, task_id.c_str(), -1, SQLITE_TRANSIENT);
    if(sqlite3_step(stmt) != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return false;
    }

    task.task_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    task.warehouse_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    task.task_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    task.state = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

    std::string robotIds = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    task.robot_ids.clear();
    if(!robotIds.empty())
    {
        auto json = nlohmann::json::parse(robotIds, nullptr, false);
        if(json.is_array())
        {
            for(const auto& item : json)
                task.robot_ids.push_back(item.get<std::string>());
        }
    }

    if(sqlite3_column_text(stmt, 5))
        task.goal = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    if(sqlite3_column_text(stmt, 6))
        task.reason = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    if(sqlite3_column_text(stmt, 7))
        task.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
    if(sqlite3_column_text(stmt, 8))
        task.updated_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));

    sqlite3_finalize(stmt);

    const char* goodsSql = R"(
        SELECT goods_id, goods_name, total_quantity
        FROM tbl_task_goods
        WHERE task_id=?;
    )";

    sqlite3_stmt* goodsStmt = nullptr;
    if(sqlite3_prepare_v2(m_pDB, goodsSql, -1, &goodsStmt, nullptr) == SQLITE_OK)
    {
        sqlite3_bind_text(goodsStmt, 1, task_id.c_str(), -1, SQLITE_TRANSIENT);
        while(sqlite3_step(goodsStmt) == SQLITE_ROW)
        {
            TaskGoods goods;
            goods.goods_id = reinterpret_cast<const char*>(sqlite3_column_text(goodsStmt, 0));
            if(sqlite3_column_text(goodsStmt, 1))
                goods.goods_name = reinterpret_cast<const char*>(sqlite3_column_text(goodsStmt, 1));
            goods.total_quantity = sqlite3_column_int(goodsStmt, 2);
            task.goods.push_back(goods);
        }
        sqlite3_finalize(goodsStmt);
    }

    return true;
}

bool SQLiteDB::GetActiveTasks(std::vector<TransportTask>& tasks)
{
    std::lock_guard<std::mutex> lock(m_Mu);
    if(!m_pDB)
        return false;

    const char* sql = R"(
        SELECT task_id, warehouse_id, task_type, state, robot_ids
        FROM tbl_task
        WHERE state IN ('PENDING', 'ACCEPTED', 'RUNNING');
    )";

    sqlite3_stmt* stmt = nullptr;
    if(sqlite3_prepare_v2(m_pDB, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    while(sqlite3_step(stmt) == SQLITE_ROW)
    {
        TransportTask task;
        task.task_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        task.warehouse_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        task.task_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        task.state = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

        std::string robotIds = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        auto json = nlohmann::json::parse(robotIds, nullptr, false);
        if(json.is_array())
        {
            for(const auto& item : json)
                task.robot_ids.push_back(item.get<std::string>());
        }
        tasks.push_back(task);
    }

    sqlite3_finalize(stmt);
    return true;
}

bool SQLiteDB::UpdateTaskState(const std::string& task_id, const std::string& state, const std::string& reason)
{
    std::lock_guard<std::mutex> lock(m_Mu);
    if(!m_pDB)
        return false;

    const char* updateSql = R"(
        UPDATE tbl_task
        SET state=?, reason=?, updated_at=CURRENT_TIMESTAMP
        WHERE task_id=?;
    )";

    sqlite3_stmt* stmt = nullptr;
    if(sqlite3_prepare_v2(m_pDB, updateSql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text(stmt, 1, state.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, reason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, task_id.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool SQLiteDB::GetTaskGoodsStatus(const std::string& task_id, std::vector<TaskGoodsStatus>& statusList)
{
    std::lock_guard<std::mutex> lock(m_Mu);
    if(!m_pDB)
        return false;

    const char* sql = R"(
        SELECT
            goods_id,
            total_quantity,
            in_progress_quantity,
            completed_quantity,
            pending_quantity,
            updated_at
        FROM tbl_task_goods_status
        WHERE task_id=?;
    )";

    sqlite3_stmt* stmt = nullptr;
    if(sqlite3_prepare_v2(m_pDB, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text(stmt, 1, task_id.c_str(), -1, SQLITE_TRANSIENT);
    while(sqlite3_step(stmt) == SQLITE_ROW)
    {
        TaskGoodsStatus status;
        status.goods_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        status.total_quantity = sqlite3_column_int(stmt, 1);
        status.in_progress_quantity = sqlite3_column_int(stmt, 2);
        status.completed_quantity = sqlite3_column_int(stmt, 3);
        status.pending_quantity = sqlite3_column_int(stmt, 4);
        if(sqlite3_column_text(stmt, 5))
            status.updated_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        statusList.push_back(status);
    }

    sqlite3_finalize(stmt);
    return !statusList.empty();
}

bool SQLiteDB::UpdateTaskStateWithEvent(const std::string& task_id, const std::string& state, const std::string& reason, const std::string& source)
{
    std::lock_guard<std::mutex> lock(m_Mu);
    if(!m_pDB)
        return false;

    BeginTransaction();

    const char* updateSql = R"(
        UPDATE tbl_task
        SET state=?, reason=?, updated_at=CURRENT_TIMESTAMP
        WHERE task_id=?;
    )";

    sqlite3_stmt* stmt = nullptr;
    if(sqlite3_prepare_v2(m_pDB, updateSql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        Rollback();
        return false;
    }

    sqlite3_bind_text(stmt, 1, state.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, reason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, task_id.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    if(!ok)
    {
        Rollback();
        return false;
    }

    const char* eventSql = R"(
        INSERT INTO tbl_task_event
        (
            task_id,
            state,
            reason,
            source,
            timestamp
        )
        VALUES(?,?,?,?,CURRENT_TIMESTAMP);
    )";

    sqlite3_stmt* eventStmt = nullptr;
    if(sqlite3_prepare_v2(m_pDB, eventSql, -1, &eventStmt, nullptr) != SQLITE_OK)
    {
        Rollback();
        return false;
    }

    sqlite3_bind_text(eventStmt, 1, task_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(eventStmt, 2, state.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(eventStmt, 3, reason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(eventStmt, 4, source.c_str(), -1, SQLITE_TRANSIENT);
    ok = sqlite3_step(eventStmt) == SQLITE_DONE;
    sqlite3_finalize(eventStmt);
    if(!ok)
    {
        Rollback();
        return false;
    }

    Commit();
    return true;
}

bool SQLiteDB::InsertAmrTaskStatusEvent(const AmrTaskStatusEvent& event)
{
    std::lock_guard<std::mutex> lock(m_Mu);
    if(!m_pDB)
        return false;

    const char* sql = R"(
        INSERT OR IGNORE INTO tbl_amr_task_status_event
        (
            message_id,
            task_id,
            robot_id,
            event_type,
            task_state,
            goods_json,
            reason,
            timestamp,
            received_at
        )
        VALUES(?,?,?,?,?,?,?,?,CURRENT_TIMESTAMP);
    )";

    sqlite3_stmt* stmt = nullptr;
    if(sqlite3_prepare_v2(m_pDB, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text(stmt, 1, event.message_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, event.task_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, event.robot_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, event.event_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, event.task_state.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, event.goods_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, event.reason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, event.timestamp.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool SQLiteDB::ApproveGoodsAllocation(
    const std::string& request_id,
    const std::string& task_id,
    const std::string& goods_id,
    const std::string& robot_id,
    int quantity,
    const std::string& allocated_at)
{
    std::lock_guard<std::mutex> lock(m_Mu);
    if(!m_pDB)
        return false;

    BeginTransaction();

    const char* updateStatusSql = R"(
        UPDATE tbl_task_goods_status
        SET
            in_progress_quantity = in_progress_quantity + ?,
            pending_quantity = pending_quantity - ?,
            updated_at = ?
        WHERE task_id=? AND goods_id=? AND pending_quantity >= ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    if(sqlite3_prepare_v2(m_pDB, updateStatusSql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        Rollback();
        return false;
    }

    sqlite3_bind_int(stmt, 1, quantity);
    sqlite3_bind_int(stmt, 2, quantity);
    sqlite3_bind_text(stmt, 3, allocated_at.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, task_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, goods_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, quantity);
    bool updated = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(m_pDB) > 0;
    sqlite3_finalize(stmt);
    if(!updated)
    {
        Rollback();
        return false;
    }

    const char* insertAllocSql = R"(
        INSERT INTO tbl_task_goods_allocation
        (
            request_id,
            task_id,
            goods_id,
            robot_id,
            quantity,
            state,
            allocated_at
        )
        VALUES(?,?,?,?,?,'IN_PROGRESS',?);
    )";

    sqlite3_stmt* allocStmt = nullptr;
    if(sqlite3_prepare_v2(m_pDB, insertAllocSql, -1, &allocStmt, nullptr) != SQLITE_OK)
    {
        Rollback();
        return false;
    }

    sqlite3_bind_text(allocStmt, 1, request_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(allocStmt, 2, task_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(allocStmt, 3, goods_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(allocStmt, 4, robot_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(allocStmt, 5, quantity);
    sqlite3_bind_text(allocStmt, 6, allocated_at.c_str(), -1, SQLITE_TRANSIENT);
    bool inserted = sqlite3_step(allocStmt) == SQLITE_DONE;
    sqlite3_finalize(allocStmt);
    if(!inserted)
    {
        Rollback();
        return false;
    }

    Commit();
    return true;
}

bool SQLiteDB::SettleGoodsAllocation(
    const std::string& request_id,
    const std::string& robot_id,
    const std::string& state,
    const std::string& settled_at)
{
    std::lock_guard<std::mutex> lock(m_Mu);
    if(!m_pDB)
        return false;

    if(state != "COMPLETED" && state != "ABORTED")
        return false;

    const char* selectSql = R"(
        SELECT task_id, goods_id, quantity
        FROM tbl_task_goods_allocation
        WHERE request_id=? AND robot_id=? AND state='IN_PROGRESS';
    )";

    sqlite3_stmt* selectStmt = nullptr;
    if(sqlite3_prepare_v2(m_pDB, selectSql, -1, &selectStmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text(selectStmt, 1, request_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(selectStmt, 2, robot_id.c_str(), -1, SQLITE_TRANSIENT);
    if(sqlite3_step(selectStmt) != SQLITE_ROW)
    {
        sqlite3_finalize(selectStmt);
        return false;
    }

    std::string task_id = reinterpret_cast<const char*>(sqlite3_column_text(selectStmt, 0));
    std::string goods_id = reinterpret_cast<const char*>(sqlite3_column_text(selectStmt, 1));
    int quantity = sqlite3_column_int(selectStmt, 2);
    sqlite3_finalize(selectStmt);

    BeginTransaction();

    const char* updateAllocSql = R"(
        UPDATE tbl_task_goods_allocation
        SET state=?, settled_at=?
        WHERE request_id=? AND robot_id=? AND state='IN_PROGRESS';
    )";

    sqlite3_stmt* updateAllocStmt = nullptr;
    if(sqlite3_prepare_v2(m_pDB, updateAllocSql, -1, &updateAllocStmt, nullptr) != SQLITE_OK)
    {
        Rollback();
        return false;
    }

    sqlite3_bind_text(updateAllocStmt, 1, state.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(updateAllocStmt, 2, settled_at.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(updateAllocStmt, 3, request_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(updateAllocStmt, 4, robot_id.c_str(), -1, SQLITE_TRANSIENT);
    bool allocationUpdated = sqlite3_step(updateAllocStmt) == SQLITE_DONE && sqlite3_changes(m_pDB) > 0;
    sqlite3_finalize(updateAllocStmt);
    if(!allocationUpdated)
    {
        Rollback();
        return false;
    }

    const char* updateStatusSql = state == "COMPLETED" ?
        R"(
            UPDATE tbl_task_goods_status
            SET
                in_progress_quantity = in_progress_quantity - ?,
                completed_quantity = completed_quantity + ?,
                updated_at = ?
            WHERE task_id=? AND goods_id=?;
        )" :
        R"(
            UPDATE tbl_task_goods_status
            SET
                in_progress_quantity = in_progress_quantity - ?,
                pending_quantity = pending_quantity + ?,
                updated_at = ?
            WHERE task_id=? AND goods_id=?;
        )";

    sqlite3_stmt* statusStmt = nullptr;
    if(sqlite3_prepare_v2(m_pDB, updateStatusSql, -1, &statusStmt, nullptr) != SQLITE_OK)
    {
        Rollback();
        return false;
    }

    sqlite3_bind_int(statusStmt, 1, quantity);
    sqlite3_bind_int(statusStmt, 2, quantity);
    sqlite3_bind_text(statusStmt, 3, settled_at.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statusStmt, 4, task_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statusStmt, 5, goods_id.c_str(), -1, SQLITE_TRANSIENT);
    bool statusUpdated = sqlite3_step(statusStmt) == SQLITE_DONE && sqlite3_changes(m_pDB) > 0;
    sqlite3_finalize(statusStmt);
    if(!statusUpdated)
    {
        Rollback();
        return false;
    }

    Commit();
    return true;
}

}

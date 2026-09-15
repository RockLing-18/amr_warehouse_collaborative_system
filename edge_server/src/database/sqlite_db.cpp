#include "database/sqlite_db.h"
#include <iostream>
#include <sstream>
#include "utils/LogDefine.h"
#include "sqlite/sqlite3.h"

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

bool SQLiteDB::GetapPackage(const std::string& warehouse_id, const std::string& version, MapPackage& package)
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

}
#include "database/sqlite_db.h"
#include <iostream>
#include <sstream>
#include "LogDefine.h"

extern "C"
{
#include "sqlite/sqlite3.h"
}

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

    {
        const char* sql =
        "CREATE TABLE IF NOT EXISTS joyai_accident ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "camera_id TEXT,"
        "type TEXT,"
        "path TEXT,"
        "handled INTEGER DEFAULT 0,"
        "result INTEGER DEFAULT 0,"
        "synced INTEGER DEFAULT 0,"
        "detail TEXT,"
        "eventtime TEXT,"
        "uploadtime TEXT,"
        "handletime TEXT,"
        "usetime REAL"
        ");";

        char* errMsg = nullptr;
        if(sqlite3_exec(m_pDB, sql, nullptr, nullptr, &errMsg) != SQLITE_OK)
        {
            LOG_ERROR("Create table failed: {}", errMsg);
            sqlite3_free(errMsg);
            return false;
        }
    }

    {
        const char* sql_cursor =
        "CREATE TABLE IF NOT EXISTS joyai_event_sync_cursor ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "source TEXT NOT NULL UNIQUE,"
        "last_time TEXT NOT NULL,"
        "update_time TEXT DEFAULT CURRENT_TIMESTAMP"
        ");";

        char* errMsg = nullptr;

        if (sqlite3_exec(m_pDB, sql_cursor, nullptr, nullptr, &errMsg) != SQLITE_OK)
        {
            LOG_ERROR("Create sync cursor table failed: {}", errMsg);
            sqlite3_free(errMsg);
            return false;
        }
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

}
// bool SQLiteDB::InsertTask(const AccidentTask& task)
// {
//     std::lock_guard<std::mutex> lock(m_Mu);

//     const char* sql =
//         "INSERT INTO joyai_accident (camera_id,type,path,handled,result,synced,detail,eventtime,uploadtime,handletime,usetime) "
//         "VALUES (?,?,?,?,?,?,?,?,?,?,?);";

//     sqlite3_stmt* stmt = nullptr;
//     if(sqlite3_prepare_v2(m_pDB, sql, -1, &stmt, nullptr) != SQLITE_OK)
//         return false;

//     sqlite3_bind_text(stmt, 1, task.camera_id.c_str(), -1, SQLITE_TRANSIENT);
//     sqlite3_bind_text(stmt, 2, task.type.c_str(), -1, SQLITE_TRANSIENT);
//     sqlite3_bind_text(stmt, 3, task.path.c_str(), -1, SQLITE_TRANSIENT);
//     sqlite3_bind_int(stmt, 4, task.handled);
//     sqlite3_bind_int(stmt, 5, task.result);
//     sqlite3_bind_int(stmt, 6, task.synced);
//     sqlite3_bind_text(stmt, 7, task.detail.c_str(), -1, SQLITE_TRANSIENT);
//     sqlite3_bind_text(stmt, 8, task.eventtime.c_str(), -1, SQLITE_TRANSIENT);
//     sqlite3_bind_text(stmt, 9, task.uploadtime.c_str(), -1, SQLITE_TRANSIENT);
//     sqlite3_bind_text(stmt, 10, task.handletime.c_str(), -1, SQLITE_TRANSIENT);
//     sqlite3_bind_double(stmt, 11, task.usetime);

//     bool ok = sqlite3_step(stmt) == SQLITE_DONE;
//     if (!ok)
//     {
//         LOG_ERROR("InsertTask step fail, msg:{}", sqlite3_errmsg(m_pDB));
//     }
//     sqlite3_finalize(stmt);

//     // 轻量被动刷盘，无阻塞
//     sqlite3_exec(m_pDB, "PRAGMA wal_checkpoint(PASSIVE);", nullptr, nullptr, nullptr);
//     return ok;
// }

// bool SQLiteDB::FetchAllDone(std::vector<AccidentTask>& out)
// {
//     std::lock_guard<std::mutex> lock(m_Mu);

//     const char* sql =
//         "SELECT id,camera_id,type,path,handled,result,synced,detail,eventtime,uploadtime,handletime,usetime "
//         "FROM joyai_accident WHERE handled=1 and result=1 and synced=0 ORDER BY id ASC;";

//     sqlite3_stmt* stmt = nullptr;
//     if (sqlite3_prepare_v2(m_pDB, sql, -1, &stmt, nullptr) != SQLITE_OK)
//     {
//         LOG_ERROR("获取数据失败 msg:{}", sqlite3_errmsg(m_pDB));
//         return false;
//     }

//     while (sqlite3_step(stmt) == SQLITE_ROW)
//     {
//         const unsigned char* text = nullptr;
//         AccidentTask t;
//         t.id = sqlite3_column_int(stmt, 0);

//         text = sqlite3_column_text(stmt, 1);
//         t.camera_id = text ? reinterpret_cast<const char*>(text) : "";

//         text = sqlite3_column_text(stmt, 2);
//         t.type = text ? reinterpret_cast<const char*>(text) : "";

//         text = sqlite3_column_text(stmt, 3);
//         t.path = text ? reinterpret_cast<const char*>(text) : "";

//         t.handled = sqlite3_column_int(stmt, 4);
//         t.result = sqlite3_column_int(stmt, 5);
//         t.synced = sqlite3_column_int(stmt, 6);

//         text = sqlite3_column_text(stmt, 7);
//         t.detail = text ? reinterpret_cast<const char*>(text) : "";

//         text = sqlite3_column_text(stmt, 8);
//         t.eventtime = text ? reinterpret_cast<const char*>(text) : "";

//         text = sqlite3_column_text(stmt, 9);
//         t.uploadtime = text ? reinterpret_cast<const char*>(text) : "";

//         text = sqlite3_column_text(stmt, 10);
//         t.handletime = text ? reinterpret_cast<const char*>(text) : "";

//         t.usetime = sqlite3_column_double(stmt, 11);

//         LOG_DEBUG("### row id={}, handled={}, result={}, synced={}", t.id, t.handled, t.result, t.synced);
//         out.push_back(std::move(t));
//     }

//     sqlite3_finalize(stmt);
//     return true;
// }

// bool SQLiteDB::MarkSynced(const std::vector<int>& ids)
// {
//     if (ids.empty()) return true;

//     std::lock_guard<std::mutex> lock(m_Mu);

//     // 构造 SQL：UPDATE joyai_accident SET is_synced=1 WHERE id IN (?, ?, ...);
//     std::string sql = "UPDATE joyai_accident SET synced=1 WHERE id IN (";
//     for (size_t i = 0; i < ids.size(); ++i)
//     {
//         sql += (i == 0 ? "?" : ",?");
//     }
//     sql += ");";

//     sqlite3_stmt* stmt = nullptr;
//     if (sqlite3_prepare_v2(m_pDB, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
//     {
//         LOG_ERROR("Failed to prepare MarkSynced statement: {}", sqlite3_errmsg(m_pDB));
//         return false;
//     }

//     // 绑定参数
//     for (size_t i = 0; i < ids.size(); ++i)
//     {
//         sqlite3_bind_int(stmt, static_cast<int>(i + 1), ids[i]);
//     }

//     // 执行
//     int rc = sqlite3_step(stmt);
//     if (rc != SQLITE_DONE)
//     {
//         LOG_ERROR("Failed to execute MarkSynced statement: {}", sqlite3_errmsg(m_pDB));
//         sqlite3_finalize(stmt);
//         return false;
//     }

//     sqlite3_finalize(stmt);

//     // 轻量被动刷盘，无阻塞
//     sqlite3_exec(m_pDB, "PRAGMA wal_checkpoint(PASSIVE);", nullptr, nullptr, nullptr);
//     return true;
// }

// bool SQLiteDB::DeleteByIds(const std::vector<int>& ids)
// {
//     if(ids.empty()) return true;

//     std::lock_guard<std::mutex> lock(m_Mu);

//     std::string sql = "DELETE FROM joyai_accident WHERE id IN (";
//     for(size_t i = 0; i < ids.size(); ++i)
//     {
//         sql += "?";
//         if(i != ids.size() -1) sql += ",";
//     }
//     sql += ");";

//     sqlite3_stmt* stmt = nullptr;
//     if(sqlite3_prepare_v2(m_pDB, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
//         return false;

//     for(size_t i=0; i<ids.size(); ++i)
//         sqlite3_bind_int(stmt, static_cast<int>(i+1), ids[i]);

//     bool ok = sqlite3_step(stmt) == SQLITE_DONE;
//     sqlite3_finalize(stmt);
//     return ok;
// }

// bool SQLiteDB::GetLastSyncTime(const std::string& source, std::string& out)
// {
//     std::lock_guard<std::mutex> lock(m_Mu);

//     const char* sql =
//         "SELECT last_time FROM joyai_event_sync_cursor WHERE source=? LIMIT 1;";

//     sqlite3_stmt* stmt = nullptr;

//     if (sqlite3_prepare_v2(m_pDB, sql, -1, &stmt, nullptr) != SQLITE_OK)
//         return false;

//     sqlite3_bind_text(stmt, 1, source.c_str(), -1, SQLITE_TRANSIENT);

//     if (sqlite3_step(stmt) == SQLITE_ROW)
//     {
//         const char* text = (const char*)sqlite3_column_text(stmt, 0);
//         if (text)
//             out = text;
        
//         sqlite3_finalize(stmt);
//         return true;
//     }

//     sqlite3_finalize(stmt);
//     return false;
// }

// bool SQLiteDB::UpdateSyncTime(const std::string& source, const std::string& time)
// {
//     std::lock_guard<std::mutex> lock(m_Mu);

//     const char* sql =
//         "INSERT INTO joyai_event_sync_cursor(source,last_time) "
//         "VALUES(?,?) "
//         "ON CONFLICT(source) DO UPDATE SET "
//         "last_time=excluded.last_time, update_time=CURRENT_TIMESTAMP;";

//     sqlite3_stmt* stmt = nullptr;

//     if (sqlite3_prepare_v2(m_pDB, sql, -1, &stmt, nullptr) != SQLITE_OK)
//         return false;

//     sqlite3_bind_text(stmt, 1, source.c_str(), -1, SQLITE_TRANSIENT);
//     sqlite3_bind_text(stmt, 2, time.c_str(), -1, SQLITE_TRANSIENT);

//     bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
//     if(!ok)
//     {
//         LOG_ERROR("修改失败 msg:{}", sqlite3_errmsg(m_pDB));
//         sqlite3_finalize(stmt);
//         return false;
//     }

//     sqlite3_finalize(stmt);

//     // 轻量被动刷盘，无阻塞
//     sqlite3_exec(m_pDB, "PRAGMA wal_checkpoint(PASSIVE);", nullptr, nullptr, nullptr);
//     return ok;
// }

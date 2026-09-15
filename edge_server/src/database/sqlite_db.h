#pragma once

#include <string>
#include <vector>
#include <mutex>
#include "types.h"

struct sqlite3;

namespace edge_server
{


class SQLiteDB
{
public:
    static SQLiteDB& Instance();

    // 打开数据库
    bool Open(const std::string& dbPath);

    // 初始化表
    bool InitTables();

    // map 
    bool UploadMapPackage(const MapPackage& package);
    bool GetMapPackage(const std::string& warehouse_id, const std::string& version, MapPackage& package);
    bool GetActiveMap(const std::string& warehouse_id, MapPackage& package);
    bool SetActiveMap(const std::string& warehouse_id, int64_t package_id, const std::string& version);

private:
    SQLiteDB();
    ~SQLiteDB();

    SQLiteDB(const SQLiteDB&) = delete;
    SQLiteDB& operator=(const SQLiteDB&) = delete;

    bool InitMapTables();

    /*
     * 以下函数内部调用
     * 不加锁
     */
    bool InsertMapPackageInternal(const MapPackage& package, int64_t& package_id);
    bool SetActiveMapInternal(const std::string& warehouse_id, int64_t package_id, const std::string& version);
    void BeginTransaction();
    void Commit();
    void Rollback();

private:
    sqlite3* m_pDB = nullptr;
    std::mutex m_Mu;
};

}
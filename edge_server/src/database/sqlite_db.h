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
    bool initMapTables();

    bool InsertMapPackage(const MapPackage& package);

    bool GetMapPackage(const std::string& version, MapPackage& package);

    bool GetActiveMap(MapPackage& package);

    bool SetActiveMap(const std::string& version);

private:
    SQLiteDB();
    ~SQLiteDB();

    SQLiteDB(const SQLiteDB&) = delete;
    SQLiteDB& operator=(const SQLiteDB&) = delete;

private:
    sqlite3* m_pDB = nullptr;
    std::mutex m_Mu;
};

}
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

    // task
    bool CreateTask(const TransportTask& task);
    bool GetTask(const std::string& task_id, TransportTask& task);
    bool GetActiveTasks(std::vector<TransportTask>& tasks);
    bool TaskExists(const std::string& task_id);
    bool UpdateTaskState(const std::string& task_id, const std::string& state, const std::string& reason);
    bool GetTaskGoodsStatus(const std::string& task_id, std::vector<TaskGoodsStatus>& statusList);
    bool UpdateTaskStateWithEvent(const std::string& task_id, const std::string& state, const std::string& reason, const std::string& source);
    bool InsertAmrTaskStatusEvent(const AmrTaskStatusEvent& event);
    bool ApproveGoodsAllocation(const std::string& request_id, const std::string& task_id, const std::string& goods_id, const std::string& robot_id, int quantity, const std::string& allocated_at);
    bool SettleGoodsAllocation(const std::string& request_id, const std::string& robot_id, const std::string& state, const std::string& settled_at);

private:
    SQLiteDB();
    ~SQLiteDB();

    SQLiteDB(const SQLiteDB&) = delete;
    SQLiteDB& operator=(const SQLiteDB&) = delete;

    bool InitMapTables();
    bool InitTaskTables();

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

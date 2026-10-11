#include "service/task_service.h"
#include "database/sqlite_db.h"
#include "utils/LogDefine.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace edge_server
{

void TaskService::init(const std::string& warehouse_id)
{
    m_warehouse_id = warehouse_id;
}

std::string TaskService::nowIso8601()
{
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&tt, &tm);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S%z");
    return oss.str();
}

bool TaskService::createTask(const TransportTask& task, int& code, std::string& message)
{
    if(task.task_id.empty())
    {
        code = 40001;
        message = "task_id empty";
        return false;
    }

    if(task.task_type != "LOAD" && task.task_type != "UNLOAD")
    {
        code = 40002;
        message = "task_type invalid";
        return false;
    }

    if(task.robot_ids.empty())
    {
        code = 41003;
        message = "robot_ids empty";
        return false;
    }

    if(task.goods.empty())
    {
        code = 40002;
        message = "goods empty";
        return false;
    }

    for(const auto& goods : task.goods)
    {
        if(goods.goods_id.empty() || goods.total_quantity <= 0)
        {
            code = 40002;
            message = "goods invalid";
            return false;
        }
    }

    auto& db = SQLiteDB::Instance();
    if(db.TaskExists(task.task_id))
    {
        code = 41002;
        message = "task already exists";
        return false;
    }

    TransportTask insertTask = task;
    insertTask.warehouse_id = m_warehouse_id;
    insertTask.state = "PENDING";
    insertTask.created_at = nowIso8601();
    insertTask.updated_at = insertTask.created_at;

    if(!db.CreateTask(insertTask))
    {
        code = 50001;
        message = "create task failed";
        return false;
    }

    code = 0;
    message = "success";
    return true;
}

bool TaskService::getTask(const std::string& task_id, TransportTask& task)
{
    return SQLiteDB::Instance().GetTask(task_id, task);
}

bool TaskService::cancelTask(const std::string& task_id, int& code, std::string& message)
{
    TransportTask task;
    if(!getTask(task_id, task))
    {
        code = 41005;
        message = "task not found";
        return false;
    }

    if(task.state == "SUCCEEDED" || task.state == "FAILED" ||
       task.state == "CANCELLED" || task.state == "INTERRUPTED")
    {
        code = 41006;
        message = "task already in terminal state";
        return false;
    }

    if(!SQLiteDB::Instance().UpdateTaskState(task_id, "CANCELLED", "canceled by API"))
    {
        code = 50001;
        message = "update task failed";
        return false;
    }

    code = 0;
    message = "cancel requested";
    return true;
}

}

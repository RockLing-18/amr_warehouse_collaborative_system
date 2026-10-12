#include "service/task_service.h"
#include "database/sqlite_db.h"
#include "utils/LogDefine.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include "nlohmann/json.hpp"
#include "mqtt/mqtt_topic.h"

namespace edge_server
{
using json = nlohmann::json;

void TaskService::init(const std::string& warehouse_id)
{
    m_warehouse_id = warehouse_id;
}

void TaskService::setPublishCallback(PublishCallback callback)
{
    m_publish_callback = std::move(callback);
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

bool TaskService::handleGoodsApply(const GoodsApplyRequest& request, GoodsApplyResult& result)
{
    if(request.task_id.empty() || request.robot_id.empty() || request.goods.empty())
    {
        result.accepted = false;
        result.reason = "invalid request";
        return false;
    }

    for(const auto& goods : request.goods)
    {
        if(goods.goods_id.empty() || goods.total_quantity <= 0)
        {
            result.accepted = false;
            result.reason = "invalid goods";
            return false;
        }
    }

    auto& db = SQLiteDB::Instance();
    TransportTask task;
    if(!db.GetTask(request.task_id, task))
    {
        result.accepted = false;
        result.reason = "task not found";
        return false;
    }

    if(std::find(task.robot_ids.begin(), task.robot_ids.end(), request.robot_id) == task.robot_ids.end())
    {
        result.accepted = false;
        result.reason = "robot not in task";
        return false;
    }

    for(const auto& goods : request.goods)
    {
        if(!db.ApproveGoodsAllocation(
            request.request_id,
            request.task_id,
            goods.goods_id,
            request.robot_id,
            goods.total_quantity,
            nowIso8601()))
        {
            result.accepted = false;
            result.reason = "approve failed";
            return false;
        }
    }

    result.accepted = true;
    result.goods = request.goods;
    return true;
}

bool TaskService::handleAmrTaskStatus(const AmrTaskStatusEvent& event)
{
    auto& db = SQLiteDB::Instance();
    if(!db.InsertAmrTaskStatusEvent(event))
    {
        LOG_WARN("duplicate amr task status event message_id=%s", event.message_id.c_str());
        return false;
    }

    if(event.event_type == "CARRY_COMPLETED" || event.event_type == "CARRY_ABORTED")
    {
        if(event.request_id.empty())
        {
            LOG_ERROR("carry event missing request_id");
            return false;
        }

        std::string settleState =
            event.event_type == "CARRY_COMPLETED" ? "COMPLETED" : "ABORTED";
        if(!db.SettleGoodsAllocation(
            event.request_id,
            event.robot_id,
            settleState,
            event.timestamp))
        {
            LOG_ERROR("settle allocation failed request_id=%s", event.request_id.c_str());
            return false;
        }
    }

    return true;
}

std::string TaskService::buildTaskStateMessage(const std::string& task_id)
{
    TransportTask task;
    if(!SQLiteDB::Instance().GetTask(task_id, task))
        return "";

    std::vector<TaskGoodsStatus> statusList;
    if(!SQLiteDB::Instance().GetTaskGoodsStatus(task_id, statusList))
        return "";

    json goodsStatus = json::array();
    int total = 0;
    int inProgress = 0;
    int completed = 0;
    int pending = 0;

    for(const auto& status : statusList)
    {
        json item;
        item["goods_id"] = status.goods_id;
        item["goods_name"] = "";
        item["total_quantity"] = status.total_quantity;
        item["in_progress_quantity"] = status.in_progress_quantity;
        item["completed_quantity"] = status.completed_quantity;
        item["pending_quantity"] = status.pending_quantity;
        goodsStatus.push_back(item);

        total += status.total_quantity;
        inProgress += status.in_progress_quantity;
        completed += status.completed_quantity;
        pending += status.pending_quantity;
    }

    json root;
    root["warehouse_id"] = task.warehouse_id;
    root["task_id"] = task.task_id;
    root["state"] = task.state;
    root["total_quantity"] = total;
    root["in_progress_quantity"] = inProgress;
    root["completed_quantity"] = completed;
    root["pending_quantity"] = pending;
    root["goods_status"] = goodsStatus;
    root["updated_at"] = task.updated_at;
    return root.dump();
}

}

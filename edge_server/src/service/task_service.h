#pragma once

#include <string>
#include <functional>
#include "types.h"

namespace edge_server
{

class TaskService
{
public:
    void init(const std::string& warehouse_id);

    bool createTask(const TransportTask& task, int& code, std::string& message);
    bool getTask(const std::string& task_id, TransportTask& task);
    bool cancelTask(const std::string& task_id, int& code, std::string& message);
    bool handleGoodsApply(const GoodsApplyRequest& request, GoodsApplyResult& result);
    bool handleAmrTaskStatus(const AmrTaskStatusEvent& event);
    std::string buildTaskStateMessage(const std::string& task_id);

    using PublishCallback = std::function<bool(const std::string& topic, const std::string& payload)>;
    void setPublishCallback(PublishCallback callback);

private:
    static std::string nowIso8601();

private:
    std::string m_warehouse_id;
    PublishCallback m_publish_callback;
};

}

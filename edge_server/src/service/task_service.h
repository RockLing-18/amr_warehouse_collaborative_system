#pragma once

#include <string>
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

private:
    static std::string nowIso8601();

private:
    std::string m_warehouse_id;
};

}

#pragma once
#include "simulation_manager/data_define.h"
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include "simulation_manager/websocket_client.h"
#include <condition_variable>

namespace simulation_manager
{

class EdgeClient
{
public:
    using NotifyCallback = std::function<void()>;
public:
    EdgeClient(const std::string &websocket_url);

    ~EdgeClient();

    bool connect();

    void setNotifyCallback(NotifyCallback callback);

    bool isConnected() const;

    std::vector<RobotInfo> getRobotList();

    void notifyDealThread();

private:
    void onMessage(const std::string& message);
    void handleRobotList(const std::string& message);
    
private:
    std::string m_websocket_url;
    WebSocketClient m_ws;
    NotifyCallback m_notify_callback;
    std::vector<RobotInfo> m_robotList;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::thread m_thread;
    std::atomic<bool> m_running{true};
    bool m_robotListUpdated{false};
};

}
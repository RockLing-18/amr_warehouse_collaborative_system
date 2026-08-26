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

    bool subscribe();

    void setNotifyCallback(NotifyCallback callback);

    bool isConnected() const;

    std::vector<RobotInfo> getRobotList();

private:
    void onMessage(const std::string& message);
    void handleRobotList(const std::string& message);

    void notifyDealThread();
    void connectionThread();
    
private:
    std::string m_websocket_url;
    WebSocketClient m_ws;
    NotifyCallback m_notify_callback;
    std::vector<RobotInfo> m_robotList;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::thread m_notify_thread;
    std::thread m_connection_thread;
    std::atomic<bool> m_running{true};
    bool m_robotListUpdated{false};
};

}
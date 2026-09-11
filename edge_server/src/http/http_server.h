#pragma once

#include <string>
#include <thread>
#include <memory>
#include "httplib/httplib.h"

namespace edge_server
{

class BootstrapService;

class HttpServer
{
public:
    HttpServer(const std::shared_ptr<BootstrapService>& bootstrapService);
    ~HttpServer();

    bool start(const std::string& host, int port);
    void stop();

private:
    void registerRoutes();

private:
    httplib::Server m_server;
    std::thread m_thread;
    std::shared_ptr<BootstrapService> m_bootstrapService;
};

}
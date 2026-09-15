#pragma once

#include <string>
#include <thread>
#include <memory>
#include "httplib/httplib.h"

namespace edge_server
{

class BootstrapService;
class MapService;

class HttpServer
{
public:
    HttpServer(const std::shared_ptr<BootstrapService>& bootstrapService, const std::shared_ptr<MapService>& mapService);
    ~HttpServer();

    bool start(const std::string& host, int port);
    void stop();

private:
    void registerRoutes();
    void registerBootstrapRoutes();
    void registerMapRoutes();
    void registerUploadMapRoute();
    void registerDownloadActiveMapRoute();
    void registerGetActiveMapRoute();
    void registerActivateMapRoute();

private:
    httplib::Server m_server;
    std::thread m_thread;
    std::shared_ptr<BootstrapService> m_bootstrapService;
    std::shared_ptr<MapService> m_mapService;
};

}
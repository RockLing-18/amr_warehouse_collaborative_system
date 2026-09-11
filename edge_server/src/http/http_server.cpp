#include "http/http_server.h"
#include "utils/LogDefine.h"
#include "service/bootstrap_service.h"

namespace edge_server
{
HttpServer::HttpServer(const std::shared_ptr<BootstrapService>& bootstrapService)
: m_bootstrapService(bootstrapService)
{
}

HttpServer::~HttpServer()
{
    stop();
}

bool HttpServer::start(const std::string& host, int port)
{
    registerRoutes();

    m_thread = std::thread(
        [this, host, port]()
        {
            LOG_INFO("HTTP server start {}:{}", host, port);
            m_server.listen(host.c_str(), port);
        });

    return true;
}

void HttpServer::stop()
{
    m_server.stop();
    if(m_thread.joinable())
        m_thread.join();
}

void HttpServer::registerRoutes()
{
    /*
     * AMR启动获取配置
     * GET
     * /api/v1/bootstrap?robot_id=robot001
     *
     */
    std::weak_ptr<BootstrapService> bootstrapService_weakPtr = m_bootstrapService;
    m_server.Get(
        "/api/amr/bootstrap",
        [bootstrapService_weakPtr](const httplib::Request& req, httplib::Response& res)
        {
            std::string robot_id;
            if(req.has_param("robot_id"))
            {
                robot_id = req.get_param_value("robot_id");
            }

            LOG_INFO("Get /api/amr/bootstrap robot_id:{}", robot_id);

            if(robot_id.empty())
            {
                res.status = 400;
                res.set_content(R"({"code":400,"msg":"robot_id empty"})", "application/json");
                return;
            }

            std::string resJson =  R"({"code":-1})";
            auto service = bootstrapService_weakPtr.lock();
            if(service)
            {
                resJson = service->getBootstrap(robot_id);
            }

            res.set_content(resJson,  "application/json");
        });
}

}
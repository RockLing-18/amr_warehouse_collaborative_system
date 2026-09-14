#include "http/http_server.h"
#include "utils/LogDefine.h"
#include "service/bootstrap_service.h"
#include "service/map_service.h"
#include "nlohmann/json.hpp"

using json=nlohmann::json;

namespace edge_server
{


HttpServer::HttpServer(const std::shared_ptr<BootstrapService>& bootstrapService, const std::shared_ptr<MapService>& mapService)
: m_bootstrapService(bootstrapService), m_mapService(mapService)
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
    registerBootstrapRoutes();
    registerMapRoutes();
}

void HttpServer::registerBootstrapRoutes()
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

void HttpServer::registerMapRoutes()
{
    auto service = m_mapService;

    /*
     * 上传地图
     *
     * POST
     * /api/maps/upload
     * 
     * ?warehouse_id=warehouse_01
     */
    m_server.Post(
        "/api/maps/upload",
        [service](const httplib::Request& req, httplib::Response& res)
        {
            std::string warehouse_id;
            if(req.has_param("warehouse_id"))
            {
                warehouse_id = req.get_param_value("warehouse_id");
            }

            LOG_INFO("Post /api/maps/upload warehouse_id:{}", warehouse_id);

            if(warehouse_id.empty())
            {
                res.status = 400;
                res.set_content(R"({"code":400,"msg":"warehouse_id empty"})", "application/json");
                return;
            }

            MapUploadRequest request;
            request.warehouse_id = warehouse_id;
            /*
             * multipart/form-data
             *
             * 后续实现
             */
            bool result = service->uploadMap(request);
            if(result)
            {
                res.set_content( R"({"code":0,"msg":"success"})", "application/json");
            }
            else
            {
                res.status = 500;
                res.set_content(R"({"code":-1,"msg":"failed"})", "application/json");
            }
        });

    /*
     * 获取当前启用的地图
     *
     * GET
     * /api/maps/active
     *
     * ?warehouse_id=warehouse_01
     */
    m_server.Get(
        "/api/maps/active",
        [service](const httplib::Request& req, httplib::Response& res)
        {
            std::string warehouse_id;
            if(req.has_param("warehouse_id"))
            {
                warehouse_id = req.get_param_value("warehouse_id");
            }

            LOG_INFO("GET /api/maps/active warehouse_id:{}", warehouse_id);

            if(warehouse_id.empty())
            {
                res.status = 400;
                res.set_content(R"({"code":400,"msg":"warehouse_id empty"})", "application/json");
                return;
            }

            MapPackage package;
            if(!service->getActiveMap(warehouse_id,  package))
            {
                res.status = 404;
                return;
            }

            json root;
            root["code"] = 0;
            root["data"]["warehouse_id"] = package.warehouse_id;
            root["data"]["version"] = package.version;
            root["data"]["package_name"] = package.package_name;
            root["data"]["package_path"] = package.package_path;
            root["data"]["package_size"] = package.package_size;
            root["data"]["download_url"] =
                "/api/maps/download?warehouse_id="
                + package.warehouse_id
                + "&version="
                + package.version;

            res.set_content(root.dump(), "application/json");
        });

    /*
     * 下载地图
     *
     * GET
     * /api/maps/download
     *
     * ?warehouse_id=warehouse001
     */
    m_server.Get(
        "/api/maps/download",
        [service](const httplib::Request& req, httplib::Response& res)
        {
            std::string warehouse_id;
            if(req.has_param("warehouse_id"))
            {
                warehouse_id = req.get_param_value("warehouse_id");
            }

            MapPackage package;
            if(!service->getActiveMap(warehouse_id,  package))
            {
                res.status = 404;
                return;
            }


            if(warehouse_id.empty())
            {
                res.status = 400;
                res.set_content(R"({"code":400,"msg":"warehouse_id empty"})", "application/json");
                return;
            }

            LOG_INFO("GET /api/maps/active warehouse_id:{}, version:{} path:{}", warehouse_id, package.package_path);

            // res.set_header(
            //     "Content-Disposition",
            //     "attachment; filename=\"map.zip\"");

            // res.set_header(
            //     "X-Map-Version",
            //     package.version);

            res.set_file_content(package.package_path, "application/zip");
        });

    /*
     * 激活地图
     *
     * POST
     *
     * /api/maps/activate
     */
    m_server.Post(
        "/api/maps/activate",
        [service](const httplib::Request& req, httplib::Response& res)
        {
            try
            {
                auto body = json::parse(req.body);
                std::string warehouse_id = body.value("warehouse_id", "");
                std::string version = body.value("version", "");
                if(warehouse_id.empty() || version.empty())
                {
                    res.status=400;
                    res.set_content(R"({"code":400,"msg":"param empty"})", "application/json");
                    return;
                }

                LOG_INFO("POST /api/maps/activate warehouse:{} version:{}", warehouse_id, version);
                bool result = service->activateMap(warehouse_id, version);
                if(result)
                {
                    res.set_content( R"({"code":0,"msg":"success"})", "application/json");
                }
                else
                {
                    res.status = 500;
                    res.set_content( R"({"code":-1,"msg":"activate failed"})", "application/json");
                }
            }
            catch(const std::exception& e)
            {
                LOG_ERROR("parse activate map json failed:{}", e.what());
                res.status=400;
                res.set_content(R"({"code":400,"msg":"invalid json"})", "application/json");
            }

        });


}

}
#include "http/http_server.h"
#include "utils/LogDefine.h"
#include "service/bootstrap_service.h"
#include "service/map_service.h"
#include "nlohmann/json.hpp"
#include <fstream>

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
            m_server.set_payload_max_length(100 * 1024 * 1024); //100MB
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

            std::string response =  R"({"code":-1})";
            auto service = bootstrapService_weakPtr.lock();
            if(service)
            {
                response = service->getBootstrap(robot_id);
            }

            res.set_content(response,  "application/json");
        });
}

void HttpServer::registerMapRoutes()
{
    registerUploadMapRoute();
    registerDownloadActiveMapRoute();
    registerGetActiveMapRoute();
    registerActivateMapRoute();
}

void HttpServer::registerUploadMapRoute()
{
    auto service = m_mapService;

    /*
     * 上传地图
     *
     * POST
     * /api/maps/upload
     *
     * ?warehouse_id=warehouse001
     * &version=version001
     * &activate=true
     */

    m_server.Post(
        "/api/maps/upload",
        [service](const httplib::Request& req, httplib::Response& res, const httplib::ContentReader& reader)
        {
            try
            {
                // 1. 参数
                std::string warehouse_id;
                if(req.has_param("warehouse_id"))
                {
                    warehouse_id = req.get_param_value("warehouse_id");
                }

                std::string version;

                if(req.has_param("version"))
                {
                    version = req.get_param_value("version");
                }

                bool activate = false;
                if(req.has_param("activate"))
                {
                    activate = req.get_param_value("activate")  == "true";
                }

                if(warehouse_id.empty() || version.empty())
                {
                    res.status = 400;
                    res.set_content(R"({"code":400,"msg":"param empty"})", "application/json");
                    return;
                }

                // 2. 临时目录
                std::string tmp_dir = "/opt/amr/data/upload_tmp";
                fs::create_directories(tmp_dir);
                std::string tmp_file =  tmp_dir  + "/"   + warehouse_id  + "_"  + version + ".zip";
                std::ofstream ofs(tmp_file, std::ios::binary);
                if(!ofs)
                {
                    res.status = 500;
                    res.set_content(R"({"code":500,"msg":"open file failed"})", "application/json");
                    return;
                }

                uint64_t file_size = 0;

                bool write_ok = true;

                /*
                 * multipart
                 *
                 * 文件header
                 */
                auto header_cb =
                    [&](const httplib::FormData& file)
                    {
                        LOG_INFO("upload file:{}",  file.filename);
                        return true;
                    };



                // 文件内容
                auto content_cb =
                    [&](const char* data,  size_t length)
                    {
                        ofs.write(data, length);
                        if(!ofs.good())
                        {
                            write_ok=false;
                            return false;
                        }

                        file_size += length;
                        return true;
                    };


                // 3. 开始流式接收
                bool result = reader(header_cb, content_cb);
                ofs.close();

                if(!result || !write_ok)
                {
                    fs::remove(tmp_file);

                    res.status=500;
                    res.set_content(R"({"code":500,"msg":"receive file failed"})", "application/json");
                    return;
                }

                LOG_INFO("map upload temp file:{} size:{}", tmp_file, file_size);

                // 4. 调用业务层
                MapUploadRequest request;
                request.warehouse_id = warehouse_id;
                request.version = version;
                request.package_name = fs::path(tmp_file).filename().string();
                request.package_size = file_size;
                request.activate = activate;
                request.upload_file_path = tmp_file;
                if(!service->uploadMap(request))
                {
                    fs::remove(tmp_file);
                    res.status = 500;
                    res.set_content(R"({"code":500,"msg":"upload map failed"})", "application/json");
                    return;
                }

                // 5. 返回
                json response;
                response["code"] = 0;
                response["msg"] = "success";
                response["data"]["warehouse_id"] = warehouse_id;
                response["data"]["version"] = version;
                res.set_content(response.dump(), "application/json");
            }
            catch(const std::exception& e)
            {
                LOG_ERROR("upload map exception:{}",  e.what());
                res.status=500;
                res.set_content(R"({"code":500,"msg":"exception"})", "application/json");
            }
        });

}

void HttpServer::registerDownloadActiveMapRoute()
{
    auto service = m_mapService;

     /*
     * 下载地图
     *
     * GET
     * /api/maps/download
     *
     * ?.warehouse_id=warehouse001
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

            LOG_INFO("GET /api/maps/active warehouse_id:{}, path:{}", warehouse_id, package.package_path);

            // res.set_header(
            //     "Content-Disposition",
            //     "attachment; filename=\"map.zip\"");

            // res.set_header(
            //     "X-Map-Version",
            //     package.version);

            res.set_file_content(package.package_path, "application/zip");
        });
}

void HttpServer::registerGetActiveMapRoute()
{
    auto service = m_mapService;
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

            json response;
            response["code"] = 0;
            response["data"]["warehouse_id"] = package.warehouse_id;
            response["data"]["version"] = package.version;
            response["data"]["package_name"] = package.package_name;
            //response["data"]["package_path"] = package.package_path;
            response["data"]["package_size"] = package.package_size;
            // response["data"]["download_url"] =
            //     "/api/maps/download?warehouse_id="
            //     + package.warehouse_id
            //     + "&version="
            //     + package.version;

            res.set_content(response.dump(), "application/json");
        });
}

void HttpServer::registerActivateMapRoute()
{
    auto service = m_mapService;
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
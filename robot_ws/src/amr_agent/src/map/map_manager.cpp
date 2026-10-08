#include "map/map_manager.h"

#include <filesystem>
#include <fstream>
#include <system_error>
#include <cstdlib>

#include "http/http_client.h"
#include "utils/ros_logger.h"

#include "amr_common/topic_names.hpp"

namespace fs = std::filesystem;

namespace amr_agent
{


MapManager::MapManager(
    const rclcpp::Node::SharedPtr& node)
    : m_node(node)
{
}


bool MapManager::init(const std::string& path)
{
    m_mapRootPath = path;

    if (m_mapRootPath.empty())
    {
        LOG_ERROR("map root path is empty");
        return false;
    }

    auto node = m_node.lock();
    if (!node)
    {
        LOG_ERROR("node is no longer valid");
        return false;
    }

    /*
     * 创建地图目录。
     */
    std::error_code ec;

    fs::create_directories(
        m_mapRootPath,
        ec);

    if (ec)
    {
        LOG_ERROR(
            "failed to create map directory: %s",
            ec.message().c_str());

        return false;
    }

    /*
     * MapInfo 是一个“当前状态”消息。
     *
     * reliable:
     *   保证可靠传输。
     *
     * transient_local:
     *   保存最后一条消息。
     *   后启动的 navigation 节点仍然能够收到
     *   agent 之前发布的 MapInfo。
     */
    auto qos =
        rclcpp::QoS(rclcpp::KeepLast(1));

    qos.reliable();
    qos.transient_local();

    m_mapInfoPublisher =
        node->create_publisher<
            amr_interfaces::msg::MapInfo>(
                amr_common::topic::MAP_INFO,
                qos);

    /*
     * Agent 重启时，如果本地已经存在地图，
     * 先恢复地图状态。
     *
     * 注意：
     * 这里暂时不做版本管理，只要 map.yaml 存在，
     * 就认为当前本地地图有效。
     */
    if (loadCurrentMap())
    {
        LOG_INFO(
            "local map loaded successfully: %s",
            m_mapYaml.c_str());

        /*
         * 立即发布一次。
         *
         * transient_local 会保存这条消息，
         * 后启动的节点也可以拿到。
         */
        publishMapInfo();
    }
    else
    {
        LOG_INFO(
            "no valid local map found, waiting for map update");
    }

    return true;
}


void MapManager::onRegisterResponse(
    const RobotRegisterResponse& response)
{
    auto node = m_node.lock();
    if (!node)
    {
        LOG_ERROR("node is no longer valid");
        return;
    }

    if (response.code != 0)
    {
        LOG_ERROR(
            "robot register failed: %s",
            response.message.c_str());

        return;
    }

    LOG_INFO(
        "robot register success, "
        "map_version=%s, update=%s",
        response.map_version.c_str(),
        response.map_update ? "true" : "false");

    /*
     * Edge 判断当前 AMR 本地地图已经是最新的。
     */
    if (!response.map_update)
    {
        /*
         * 如果 MapManager 当前已经有有效地图，
         * 重新发布一次即可。
         */
        if (m_ready)
        {
            LOG_INFO(
                "local map is already ready, "
                "publish map info");

            publishMapInfo();
            return;
        }

        /*
         * 如果之前 init() 时没有找到地图，
         * 再检查一次本地地图。
         */
        if (loadCurrentMap())
        {
            publishMapInfo();
        }
        else
        {
            LOG_ERROR(
                "edge reports map is up-to-date, "
                "but no valid local map is available");
        }

        return;
    }

    /*
     * Edge 要求更新地图。
     */
    if (!updateMap(response))
    {
        LOG_ERROR(
            "map update failed, version=%s",
            response.map_version.c_str());

        return;
    }

    /*
     * 地图更新完成后，发布最新 MapInfo。
     */
    publishMapInfo();
}


bool MapManager::updateMap(
    const RobotRegisterResponse& response)
{
    auto node = m_node.lock();
    if (!node)
    {
        LOG_ERROR("node is no longer valid");
        return false;
    }

    if (response.map_download_url.empty())
    {
        LOG_ERROR("map download url is empty");
        return false;
    }

    /*
     * 这里暂时不在 AMR 端保存版本目录。
     *
     * 下载包放到 /tmp，
     * 地图始终解压到 m_mapRootPath。
     */
    const fs::path packagePath =
        fs::path("/tmp") /
        "amr_map_package.zip";

    std::error_code ec;

    /*
     * 删除上一次残留的地图包。
     */
    fs::remove(
        packagePath,
        ec);

    if (ec)
    {
        LOG_ERROR(
            "failed to remove old map package: %s",
            ec.message().c_str());

        return false;
    }

    LOG_INFO(
        "start downloading map: %s",
        response.map_download_url.c_str());

    if (!downloadMap(
            response.map_download_url,
            packagePath.string()))
    {
        LOG_ERROR(
            "failed to download map package");

        return false;
    }

    /*
     * 检查下载后的文件是否存在。
     */
    if (!fs::exists(packagePath, ec))
    {
        LOG_ERROR(
            "downloaded map package does not exist: %s",
            packagePath.c_str());

        return false;
    }

    /*
     * 清空当前地图目录。
     *
     * 当前设计明确采用“直接覆盖”：
     *
     * /opt/amr/data/arm_runtime/map/*
     *
     * 全部删除，然后解压最新地图。
     */
    LOG_INFO(
        "clearing local map directory: %s",
        m_mapRootPath.c_str());

    if (!fs::exists(m_mapRootPath, ec))
    {
        fs::create_directories(
            m_mapRootPath,
            ec);

        if (ec)
        {
            LOG_ERROR(
                "failed to create map directory: %s",
                ec.message().c_str());

            fs::remove(
                packagePath,
                ec);

            return false;
        }
    }
    else
    {
        for (const auto& entry :
             fs::directory_iterator(m_mapRootPath, ec))
        {
            if (ec)
            {
                LOG_ERROR(
                    "failed to iterate map directory: %s",
                    ec.message().c_str());

                fs::remove(
                    packagePath,
                    ec);

                return false;
            }

            fs::remove_all(
                entry.path(),
                ec);

            if (ec)
            {
                LOG_ERROR(
                    "failed to remove map entry %s: %s",
                    entry.path().c_str(),
                    ec.message().c_str());

                fs::remove(
                    packagePath,
                    ec);

                return false;
            }
        }
    }

    /*
     * 解压到地图根目录。
     *
     * 约定 ZIP 内部直接包含：
     *
     * map.yaml
     * map.pgm
     * zones.yaml
     * stations.yaml
     *
     * 而不是再套一层目录。
     */
    if (!extractMap(
            packagePath.string(),
            m_mapRootPath))
    {
        LOG_ERROR(
            "failed to extract map package");

        /*
         * 下载包清理。
         */
        fs::remove(
            packagePath,
            ec);

        return false;
    }

    /*
     * 删除下载的 ZIP。
     */
    fs::remove(
        packagePath,
        ec);

    if (ec)
    {
        LOG_WARN(
            "failed to remove map package: %s",
            ec.message().c_str());
    }

    /*
     * 验证 map.yaml。
     */
    const fs::path mapYaml =
        fs::path(m_mapRootPath) /
        "map.yaml";

    if (!fs::exists(mapYaml, ec))
    {
        LOG_ERROR(
            "map.yaml not found after extraction: %s",
            mapYaml.c_str());

        return false;
    }

    /*
     * 保存当前地图信息。
     */
    m_mapVersion = response.map_version;
    m_mapDir = m_mapRootPath;
    m_mapYaml = mapYaml.string();

    m_ready = true;

    LOG_INFO(
        "map updated successfully, "
        "version=%s, path=%s",
        m_mapVersion.c_str(),
        m_mapYaml.c_str());

    return true;
}


bool MapManager::downloadMap(
    const std::string& url,
    const std::string& packagePath)
{
    auto node = m_node.lock();
    if (!node)
    {
        LOG_ERROR("node is no longer valid");
        return false;
    }

    if (!m_httpClient)
    {
        LOG_ERROR(
            "http client is not initialized");

        return false;
    }

    /*
     * 使用已经初始化好的 HttpClient。
     *
     * url 当前应该是类似：
     *
     * /api/maps/download?warehouse_id=warehouse_01
     */
    const auto response =
        m_httpClient->get(url);

    if (!response.succeed)
    {
        LOG_ERROR(
            "download map failed: %s",
            response.errMsg.c_str());

        return false;
    }

    if (response.body.empty())
    {
        LOG_ERROR(
            "download map succeeded but response body is empty");

        return false;
    }

    std::ofstream ofs(
        packagePath,
        std::ios::binary);

    if (!ofs.is_open())
    {
        LOG_ERROR(
            "failed to open map package for writing: %s",
            packagePath.c_str());

        return false;
    }

    ofs.write(
        response.body.data(),
        static_cast<std::streamsize>(
            response.body.size()));

    if (!ofs.good())
    {
        LOG_ERROR(
            "failed to write map package: %s",
            packagePath.c_str());

        ofs.close();

        return false;
    }

    ofs.close();

    LOG_INFO(
        "map package downloaded: %s, size=%zu",
        packagePath.c_str(),
        response.body.size());

    return true;
}


bool MapManager::extractMap(
    const std::string& packagePath,
    const std::string& mapDir)
{
    auto node = m_node.lock();
    if (!node)
    {
        LOG_ERROR("node is no longer valid");
        return false;
    }

    if (!fs::exists(packagePath))
    {
        LOG_ERROR(
            "map package does not exist: %s",
            packagePath.c_str());

        return false;
    }

    std::error_code ec;

    fs::create_directories(
        mapDir,
        ec);

    if (ec)
    {
        LOG_ERROR(
            "failed to create map directory: %s",
            ec.message().c_str());

        return false;
    }

    /*
     * 当前第一版直接调用系统 unzip。
     *
     * packagePath 和 mapDir 都是程序自己生成的路径，
     * 不是外部用户输入。
     */
    const std::string command =
        "unzip -o \"" +
        packagePath +
        "\" -d \"" +
        mapDir +
        "\" > /dev/null";

    LOG_INFO(
        "extracting map package to: %s",
        mapDir.c_str());

    const int ret =
        std::system(command.c_str());

    if (ret != 0)
    {
        LOG_ERROR(
            "unzip failed, ret=%d",
            ret);

        return false;
    }

    LOG_INFO(
        "map package extracted successfully");

    return true;
}


bool MapManager::activateMap(
    const std::string& mapDir)
{
    /*
     * 当前版本暂时不做 AMR 端版本管理。
     *
     * 地图已经直接解压到 m_mapRootPath，
     * 因此 activateMap 不需要再做软链接切换。
     *
     * 保留这个接口，是为了以后如果恢复版本管理，
     * 可以重新实现。
     */
    if (mapDir.empty())
    {
        LOG_ERROR(
            "map directory is empty");

        return false;
    }

    if (!fs::exists(mapDir))
    {
        LOG_ERROR(
            "map directory does not exist: %s",
            mapDir.c_str());

        return false;
    }

    return true;
}


bool MapManager::loadCurrentMap()
{
    auto node = m_node.lock();
    if (!node)
    {
        LOG_ERROR("node is no longer valid");
        return false;
    }

    if (m_mapRootPath.empty())
    {
        LOG_ERROR(
            "map root path is empty");

        return false;
    }

    const fs::path mapYaml =
        fs::path(m_mapRootPath) /
        "map.yaml";

    std::error_code ec;

    if (!fs::exists(mapYaml, ec))
    {
        LOG_INFO(
            "local map.yaml does not exist: %s",
            mapYaml.c_str());

        m_ready = false;

        return false;
    }

    /*
     * 当前不做版本目录解析。
     *
     * 如果需要版本号：
     *
     * 1. 可以从 map.yaml 中读取。
     * 2. 也可以直接使用空字符串。
     *
     * 当前 edge register response 中有 map_version，
     * 如果 agent 重启但还没有重新收到 register response，
     * 这里不强行猜版本。
     */
    m_mapDir =
        m_mapRootPath;

    m_mapYaml =
        mapYaml.string();

    m_ready = true;

    LOG_INFO(
        "loaded local map: %s",
        m_mapYaml.c_str());

    return true;
}


void MapManager::publishMapInfo()
{
    auto node = m_node.lock();
    if (!node)
    {
        LOG_ERROR("node is no longer valid");
        return;
    }

    if (!m_mapInfoPublisher)
    {
        LOG_ERROR(
            "map info publisher is not initialized");

        return;
    }

    if (!m_ready)
    {
        LOG_WARN(
            "map is not ready, "
            "skip publishing map info");

        return;
    }

    amr_interfaces::msg::MapInfo msg;

    msg.warehouse_id =
        m_warehouseId;

    msg.map_version =
        m_mapVersion;

    msg.map_dir =
        m_mapDir;

    msg.map_yaml =
        m_mapYaml;

    msg.ready = true;

    m_mapInfoPublisher->publish(msg);

    LOG_INFO(
        "published map info: "
        "warehouse_id=%s, "
        "map_version=%s, "
        "map_dir=%s, "
        "map_yaml=%s",
        msg.warehouse_id.c_str(),
        msg.map_version.c_str(),
        msg.map_dir.c_str(),
        msg.map_yaml.c_str());
}


bool MapManager::isReady() const
{
    return m_ready;
}


std::string MapManager::getMapDir() const
{
    return m_mapDir;
}


std::string MapManager::getMapYaml() const
{
    return m_mapYaml;
}

}
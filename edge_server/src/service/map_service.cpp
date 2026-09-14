#include "service/map_service.h"
#include <filesystem>
#include <fstream>
#include "database/sqlite_db.h"
#include "utils/LogDefine.h"

namespace fs = std::filesystem;

namespace edge_server
{

bool MapService::uploadMap(const MapUploadRequest& request)
{
    if(!checkRequest(request))
    {
        return false;
    }

    std::string target_path;
    if(!prepareMapDirectory(request.warehouse_id, request.version, target_path))
    {
        return false;
    }

    // 保存文件
    if(!copyFile(request.source_path, target_path))
    {
        return false;
    }

    MapPackage package;
    package.warehouse_id = request.warehouse_id;
    package.version = request.version;
    package.package_name = request.package_name;
    package.package_path = target_path;
    package.package_size = fs::file_size(target_path);
    package.activate = request.activate;

    // 写数据库
    auto& db = SQLiteDB::Instance();
    if(!db.UploadMapPackage(package))
    {
        LOG_ERROR("upload map database failed");
        return false;
    }

    LOG_INFO(
        "upload map success warehouse:{} version:{}",
        package.warehouse_id,
        package.version);
    return true;
}

bool MapService::getActiveMap(const std::string& warehouse_id, MapPackage& package)
{
    auto& db = SQLiteDB::Instance();
    return db.GetActiveMap(warehouse_id, package);
}

bool MapService::activateMap(const std::string& warehouse_id, const std::string& version)
{
    MapPackage package;

    // 查询版本
    auto& db = SQLiteDB::Instance();
    if(!db.GetapPackage(warehouse_id, version, package))
    {
        LOG_ERROR("map version not found {} {}", warehouse_id,  version);
        return false;
    }

    // 设置active
    return db.SetActiveMap(warehouse_id, package.id, package.version);
}

bool MapService::checkRequest(const MapUploadRequest& request)
{
    if(request.warehouse_id.empty())
    {
        LOG_ERROR("warehouse id empty");
        return false;
    }

    if(request.version.empty())
    {
        LOG_ERROR("map version empty");
        return false;
    }

    if(request.source_path.empty())
    {
        LOG_ERROR("source path empty");
        return false;
    }

    if(!fs::exists(request.source_path))
    {
        LOG_ERROR("map file not exist {}", request.source_path);
        return false;
    }

    return true;
}

bool MapService::prepareMapDirectory(const std::string& warehouse_id, const std::string& version, std::string& target_path)
{
    fs::path root = "/opt/amr/data/warehouses";
    fs::path dir = root / warehouse_id / "maps" / version;

    try
    {
        fs::create_directories(dir);
    }
    catch(const std::exception& e)
    {
        LOG_ERROR( "create map directory failed:{}", e.what());
        return false;
    }

    target_path =  (dir / "map.zip").string();
    return true;
}

bool MapService::copyFile( const std::string& src, const std::string& dst)
{
    try
    {
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
    }
    catch(const std::exception& e)
    {
        LOG_ERROR( "copy map failed:{}", e.what());
        return false;
    }

    return true;
}

}
#pragma once

#include <string>
#include "types.h"


namespace edge_server
{

class MapService
{
public:
    MapService(const std::string& warehouseId);
    /*
     * 上传地图
     *
     * 1. 保存文件
     * 2. 保存数据库
     * 3. 根据activate决定是否启用
     */
    bool uploadMap(const MapUploadRequest& request);

    // 获取当前激活地图
    bool getActiveMap(const std::string& warehouse_id, MapPackage& package);

    // 激活指定版本
    bool activateMap( const std::string& warehouse_id, const std::string& version);

private:
    bool checkRequest(const MapUploadRequest& request);
    bool prepareMapDirectory(const std::string& warehouse_id, const std::string& version, std::string& target_path);
    bool copyFile(const std::string& src, const std::string& dst);

private:
    std::string m_warehouseId;
};


}
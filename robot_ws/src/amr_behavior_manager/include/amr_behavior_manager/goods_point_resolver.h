#pragma once

#include <memory>
#include <string>

#include "amr_behavior_manager/zone_station_loader.h"

namespace amr_behavior_manager
{

class GoodsPointResolver
{
public:
    explicit GoodsPointResolver(std::shared_ptr<ZoneStationLoader> loader);

    bool resolve(
        const std::string& goods_id,
        const std::string& task_type,
        LocationInfo& location) const;

private:
    std::shared_ptr<ZoneStationLoader> m_loader;
};

}

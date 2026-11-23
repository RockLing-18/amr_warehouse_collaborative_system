#pragma once
#include <string>
#include <vector>

struct ZoneInfo
{
    std::string id;
    std::string name;
    std::string type;
    std::vector<std::string> goods_ids;
    std::string filename;
};

struct Zone
{
    ZoneInfo info;
    std::string frame_id{"map"};
    int marker_id{-1};  // marker运行时id
    std::vector<std::pair<double,double>> polygon;
};

struct Pose2D
{
    double x;
    double y;
    double yaw;
};

struct Location
{
    std::string id;
    std::string type;
    int marker_id;
    Pose2D pose;
};

struct Station
{
    std::string id;
    std::string name;
    std::string zone_id;
    std::vector<Location> locations;
};

struct Wall
{
    int marker_id{-1};  // marker运行时id
    std::string id;
    std::vector<std::pair<double,double>> line;
};

struct TrafficGate
{
    std::string resource_id;
    std::vector<std::pair<double,double>> polygon;  // 4 点凸窄道区域
    std::vector<Pose2D> l1_wait_poses;  // l1 侧等待点
    std::vector<Pose2D> l2_wait_poses;  // l2 侧等待点
    int marker_id{-1};  // marker运行时id，不写入 yaml
};

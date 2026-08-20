#pragma once
#include <string>
#include <vector>

struct ZoneInfo
{
    std::string id;
    std::string name;
    std::string type;
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
    std::string type;
    std::string zone_id;
    std::vector<Location> locations;
};

struct Wall
{
    int marker_id{-1};  // marker运行时id
    std::string id;
    std::vector<std::pair<double,double>> line;
};
#pragma once
#include <filesystem>
#include <string>
#include <unistd.h>
#include <chrono>
#include <string_view>
#include <cstdint>

namespace fs = std::filesystem;

namespace utils
{
// 返回可执行文件所在目录，不带末尾 '/'
inline std::string get_exe_dir()
{
    char buf[1024] = {0};
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf)-1);
    if(len <=0)
    {
        return ".";
    }
    std::string exe_path(buf, static_cast<size_t>(len));
    fs::path p(exe_path);
    return p.parent_path().string();
}


inline std::string get_date_string()
{
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);

    std::tm tm{};
    localtime_r(&tt, &tm);   // Linux

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d");

    return oss.str();
}


inline std::string stripIpv4MappedPrefix(const std::string& peer)
{
    constexpr std::string_view prefix = "::ffff:";
    if (peer.size() > prefix.size() && peer.substr(0, prefix.size()) == prefix)
    {
        return peer.substr(prefix.size());
    }

    return peer;
}

inline int64_t getCurrentTimeMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

}
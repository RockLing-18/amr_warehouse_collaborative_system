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

inline int64_t getCurrentTime()
{
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}


inline int64_t getCurrentTimeMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

inline std::string ms_to_md_hms(int64_t ms_ts)
{
    time_t sec = ms_ts / 1000; // 丢弃毫秒
    struct tm tm_buf{};
    localtime_r(&sec, &tm_buf); // 本地时间；如果要UTC用 gmtime_r

    char buf[32] = {0};
    strftime(buf, sizeof(buf), "%m%d%H%M%S", &tm_buf);
    return std::string(buf);
}

}
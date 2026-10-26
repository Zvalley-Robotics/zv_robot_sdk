#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace zv::robot
{

struct FileInfo
{
    std::string name;
    std::string path;  // 远端路径，例如 "/files/maps/map.yaml"
    int64_t size{0};   // 文件大小（字节）
};

struct FileAccessInfo
{
    std::string token;
    std::string base_url;
    int64_t expires_at{0};  // Unix 时间戳
    int ttl_seconds{0};
};

// 进度回调：progress_fn(已下载字节数, 总字节数)
using DownloadProgressFn = std::function<void(int64_t, int64_t)>;

}  // namespace zv::robot

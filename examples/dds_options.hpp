#pragma once

#include <charconv>
#include <filesystem>
#include <map>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <zv/robot/channel/channel_factory.hpp>

namespace example
{
struct DdsArguments
{
    bool use_config = false;
    bool help = false;
    std::string network_interface;
    std::map<std::string, std::string> values;
};

inline int ParseInteger(const std::string & value, int minimum, int maximum)
{
    int result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc() || parsed.ptr != value.data() + value.size() || result < minimum || result > maximum)
        throw std::invalid_argument("Invalid integer: " + value);
    return result;
}

inline DdsArguments ParseDdsArguments(int argc, const char * const argv[],
                                      const std::set<std::string> & extra_keys = {})
{
    DdsArguments options;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--help") {
            options.help = true;
            continue;
        }
        if (argument == "--use_config") {
            options.use_config = true;
            continue;
        }
        const auto separator = argument.find('=');
        if (argument.rfind("--", 0) != 0 || separator == std::string::npos)
            throw std::invalid_argument("Unknown argument: " + argument);
        const auto key = argument.substr(2, separator - 2);
        const auto value = argument.substr(separator + 1);
        if (key == "network_interface") {
            if (!value.empty() && !std::regex_match(value, std::regex("[A-Za-z0-9_.:-]{1,15}")))
                throw std::invalid_argument("Invalid network interface");
            options.network_interface = value;
        } else if (extra_keys.count(key)) {
            options.values[key] = value;
        } else {
            throw std::invalid_argument("Unknown option: " + key);
        }
    }
    return options;
}

// 从可执行文件所在目录向上查找，兼容源码构建、bin/<arch>/ 和安装后的示例目录。
inline std::string DdsConfigPath()
{
    auto directory = std::filesystem::read_symlink("/proc/self/exe").parent_path();
    while (!directory.empty()) {
        const auto path = directory / "config/dds.json";
        if (std::filesystem::is_regular_file(path)) return path.string();
        // 顶层构建输出在 build/examples/，配置仍保留在源码树的 examples/config/。
        const auto source_path = directory / "examples/config/dds.json";
        if (std::filesystem::is_regular_file(source_path)) return source_path.string();
        const auto parent = directory.parent_path();
        if (parent == directory) break;
        directory = parent;
    }
    throw std::runtime_error("DDS config file not found: config/dds.json or examples/config/dds.json beside examples");
}

inline void InitDds(const std::string & network_interface, bool use_config)
{
    if (use_config) {
        zv::robot::ChannelFactory::Instance()->Init(DdsConfigPath());
    } else {
        zv::robot::ChannelFactory::Instance()->Init(0, network_interface);  // 域 id 由 ZV_DDS_DOMAIN_ID 环境变量指定。
    }
}
}  // namespace example

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <zv/robot/slam/slam_client.hpp>
#include <zv/third_party/nlohmann_json/json.hpp>

namespace fs = std::filesystem;

using namespace zv::robot;

std::vector<float> stringToFloatVector(const std::string & str)
{
    std::vector<float> result;
    std::stringstream ss(str);
    float num;
    while (ss >> num) {
        result.push_back(num);
        // ignore any trailing whitespace
        ss.ignore();
    }
    return result;
}

std::vector<std::string> stringToStringVector(const std::string & str)
{
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;
    while (ss >> item) {
        result.push_back(item);
    }
    return result;
}

// 辅助函数：从字符串数组指定偏移位置提取位姿参数 (支持 欧拉角 6位 或 四元数 7位)
NavPose parseNavPose(const std::vector<std::string> & vec, size_t offset)
{
    NavPose pose;
    auto param_count = vec.size() - offset;
    pose.position.x = std::stof(vec.at(offset + 0));
    pose.position.y = std::stof(vec.at(offset + 1));
    pose.position.z = std::stof(vec.at(offset + 2));
    if (param_count >= 7) {  // 优先处理四元数
        pose.orientation.q_x = std::stof(vec.at(offset + 3));
        pose.orientation.q_y = std::stof(vec.at(offset + 4));
        pose.orientation.q_z = std::stof(vec.at(offset + 5));
        pose.orientation.q_w = std::stof(vec.at(offset + 6));
    } else if (param_count >= 6) {  // 否则处理欧拉角
        pose.euler.r = std::stof(vec.at(offset + 3));
        pose.euler.p = std::stof(vec.at(offset + 4));
        pose.euler.y = std::stof(vec.at(offset + 5));
    }
    return pose;
}

// 辅助函数：校验参数长度并提取 NavPose，包含异常处理
bool extractNavPoseParam(const std::vector<std::string> & param_vec, size_t offset, NavPose & pose,
                         const std::string & cmd_name)
{
    auto param_size = param_vec.size();
    if (param_size == offset) {
        return true;
    } else if (param_size == offset + 6 || param_size == offset + 7) {
        try {
            pose = parseNavPose(param_vec, offset);
            return true;
        } catch (const std::exception & e) {
            std::cerr << "Parse " << cmd_name << " param error: " << e.what() << std::endl;
            return false;
        }
    } else {
        std::cerr << "Invalid param size for " << cmd_name << ": " << param_size << std::endl;
        return false;
    }
}

// 辅助函数：打印命令执行结果
void printCmdResult(const std::string & cmd, int32_t ret, const std::string & data = "")
{
    if (ret == 0) {
        std::cout << "Command '" << cmd << "' accepted (0)!" << std::endl;
    } else if (ret == 1) {
        std::cout << "Command '" << cmd << "' executed successfully (1)!" << std::endl;
    } else if (ret == 2) {
        std::cerr << "Command '" << cmd << "' execution failed (2)!" << std::endl;
    } else {
        std::cerr << "Command '" << cmd << "' execution failed, error code: " << ret << std::endl;
    }
    if (!data.empty()) {
        (ret == 0 || ret == 1 ? std::cout : std::cerr) << "Response Data: " << data << std::endl;
    }
}

// 辅助函数：处理简单的无参数动作命令
void handleSimpleActionCommand(SlamClient & client, const std::string & cmd_str,
                               std::function<int32_t(std::string &)> client_call)
{
    std::string data;
    int32_t ret = client_call(data);
    printCmdResult(cmd_str, ret, data);
}

int main(int argc, char const * argv[])
{
    std::map<std::string, std::string> args = {
        {"network_interface", "lo"}
    };

    std::map<std::string, std::string> values;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.substr(0, 2) == "--") {
            size_t pos = arg.find("=");
            std::string key, value;
            if (pos != std::string::npos) {
                key = arg.substr(2, pos - 2);
                value = arg.substr(pos + 1);

                if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                    value = value.substr(1, value.length() - 2);
                }
            } else {
                key = arg.substr(2);
                value = "";
            }
            if (args.find(key) != args.end()) {
                args[key] = value;
            } else {
                args.insert({
                    {key, value}
                });
            }
        }
    }

    if (argc == 1 || args.count("help")) {
        std::cout << "Usage: slam [--network_interface=<iface>] <command[=args]>\n"
                     "\n"
                     "建图命令:\n"
                     "  --start_mapping=<scene_id> <type>   开启建图 (type: indoor|outdoor)\n"
                     "  --stop_mapping=<scene_id>            停止建图并保存\n"
                     "  --terminate_mapping=<scene_id>       强制终止建图，不保存\n"
                     "  --query_map                          查询机器人中已有的地图列表\n"
                     "  --preview_map=<scene_id>             预览指定地图\n"
                     "  --delete_map=<scene_id>              删除指定地图\n"
                     "  --download_maps[=<local_dir>]        下载所有地图到本地目录（默认 .）\n"
                     "\n"
                     "导航命令:\n"
                     "  --init_pose=<scene_id> <auto> [x y z r p yaw]   初始化位姿并启动定位\n"
                     "  --start_nav=<relative> <x y z r p yaw>          开始导航到目标位姿\n"
                     "  --get_state                          查询当前状态（空闲/建图/就绪/导航中）\n"
                     "  --get_pose                           查询当前位姿\n"
                     "  --pause_nav                          暂停导航\n"
                     "  --resume_nav                         恢复导航\n"
                     "  --cancel_nav                         取消导航\n"
                     "  --restart_odom[=true|false]           重启里程计（默认重定位）\n"
                     "  --shutdown                           结束定位与导航\n"
                     "\n"
                     "示例:\n"
                     "  slam --start_mapping=\"slam_test indoor\"\n"
                     "  slam --stop_mapping=\"slam_test\"\n"
                     "  slam --init_pose=\"slam_test true\"\n"
                     "  slam --init_pose=\"slam_test false 1.5 3.0 0.0 0.0 0.0 0.0\"\n"
                     "  slam --start_nav=\"false 1.5 3.0 0.0 0.0 0.0 0.0\"\n";
        return 0;
    }

    zv::robot::ChannelFactory::Instance()->Init(0);

    SlamClient slam_client;
    slam_client.Init();

    slam_client.SetTimeout(10.f);

    for (const auto & arg_pair : args) {
        std::cout << "Processing command: [" << arg_pair.first << "] with param: [" << arg_pair.second << "] ..."
                  << std::endl;
        if (arg_pair.first == "network_interface") {
            continue;
        }

        const std::string & cmd = arg_pair.first;
        const std::string & param = arg_pair.second;

        if (cmd == "start_mapping") {
            std::vector<std::string> param_vec = stringToStringVector(param);
            auto param_size = param_vec.size();
            std::string scene_id, slam_type;
            if (param_size == 2) {
                scene_id = param_vec.at(0);
                slam_type = param_vec.at(1);
            } else if (param_size != 0) {
                std::cerr << "Invalid param size for start_mapping: " << param_size << std::endl;
                continue;
            }
            std::string data;
            int32_t ret = slam_client.StartMapping(scene_id, slam_type, data);
            printCmdResult(cmd, ret, data);
        } else if (cmd == "stop_mapping" || cmd == "terminate_mapping") {
            std::vector<std::string> param_vec = stringToStringVector(param);
            auto param_size = param_vec.size();
            std::string scene_id;
            if (param_size == 1) {
                scene_id = param_vec.at(0);
            } else if (param_size != 0) {
                std::cerr << "Invalid param size for stop_mapping: " << param_size << std::endl;
                continue;
            }
            std::string data;
            int32_t ret = cmd == "stop_mapping" ? slam_client.StopMapping(scene_id, data)
                                                : slam_client.TerminateMapping(scene_id, data);
            printCmdResult(cmd, ret, data);
        } else if (cmd == "init_pose") {
            std::vector<std::string> param_vec = stringToStringVector(param);
            std::string scene_id;
            bool auto_init = true;
            NavPose manual_init_pose;

            if (!extractNavPoseParam(param_vec, 2, manual_init_pose, cmd)) {
                continue;
            }
            scene_id = param_vec.at(0);
            auto_init = param_vec.at(1) == "true";

            std::string data;
            int32_t ret = slam_client.InitPose(scene_id, auto_init, manual_init_pose, data);
            printCmdResult(cmd, ret, data);
        } else if (cmd == "get_state") {
            std::string nav_state;
            int32_t ret = slam_client.GetState(nav_state);
            if (ret == 0 || ret == 1) {
                std::cout << "Nav State: " << nav_state << std::endl;
            } else {
                std::cerr << "Command '" << cmd << "' execution failed, error code: " << ret << std::endl;
            }
        } else if (cmd == "start_nav") {
            std::vector<std::string> param_vec = stringToStringVector(param);
            bool relative = true;
            NavPose target_pose;

            if (!extractNavPoseParam(param_vec, 1, target_pose, cmd)) {
                continue;
            }
            relative = param_vec.at(0) == "true";

            std::string data;
            int32_t ret = slam_client.PoseNav(relative, target_pose, data);
            printCmdResult(cmd, ret, data);
        } else if (cmd == "cancel_nav") {
            handleSimpleActionCommand(slam_client, cmd, [&](std::string & d) { return slam_client.CancelNav(d); });
        } else if (cmd == "pause_nav") {
            handleSimpleActionCommand(slam_client, cmd, [&](std::string & d) { return slam_client.PauseNav(d); });
        } else if (cmd == "resume_nav") {
            handleSimpleActionCommand(slam_client, cmd, [&](std::string & d) { return slam_client.ResumeNav(d); });
        } else if (cmd == "restart_odom") {
            if (!param.empty() && param != "true" && param != "false") {
                std::cerr << "restart_odom expects true or false" << std::endl;
                continue;
            }
            std::string data;
            printCmdResult(cmd, slam_client.RestartOdom("", param != "false", data), data);
        } else if (cmd == "query_map") {
            std::vector<zv::robot::FileInfo> files;
            int32_t ret = slam_client.QueryMap(files);
            if (ret == 0) {
                std::cout << "Found " << files.size() << " map file(s):" << std::endl;
                for (const auto & f : files) {
                    std::cout << "  " << f.name << "  (" << f.size / 1024 << " KB)"
                              << "  path=" << f.path << std::endl;
                }
            } else {
                printCmdResult(cmd, ret);
            }
        } else if (cmd == "preview_map") {
            std::string map_id;
            std::vector<std::string> param_vec = stringToStringVector(param);
            if (param_vec.size() == 1) {
                map_id = param_vec.at(0);
            } else if (!param_vec.empty()) {
                std::cerr << "Invalid param size for preview_map: " << param_vec.size() << std::endl;
                continue;
            }
            std::string map_data;
            int32_t ret = slam_client.PreviewMap(map_id, map_data);
            if (ret == 0 || ret == 1) {
                std::cout << "Map Data: " << map_data << std::endl;
            } else {
                printCmdResult(cmd, ret, map_data);
            }
        } else if (cmd == "delete_map") {
            std::string map_id;
            std::vector<std::string> param_vec = stringToStringVector(param);
            if (param_vec.size() == 1) {
                map_id = param_vec.at(0);
            } else if (!param_vec.empty()) {
                std::cerr << "Invalid param size for delete_map: " << param_vec.size() << std::endl;
                continue;
            }
            std::string data;
            int32_t ret = slam_client.DeleteMap(map_id, data);
            printCmdResult(cmd, ret, data);
        } else if (cmd == "shutdown") {
            handleSimpleActionCommand(slam_client, cmd, [&](std::string & d) { return slam_client.Close(d); });
        } else if (cmd == "download_maps") {
            // 用法：--download_maps=<本地目录>
            std::string local_dir = param.empty() ? "." : param;

            std::error_code ec;
            fs::create_directories(local_dir, ec);
            if (ec) {
                std::cerr << "Cannot create directory '" << local_dir << "': " << ec.message() << std::endl;
                continue;
            }

            int ok = 0, fail = 0;
            int32_t ret = slam_client.DownloadMaps(local_dir, &ok, &fail);
            if (ret != 0 && fail == 0) {
                std::cerr << "download_maps failed (code=" << ret << ")" << std::endl;
            } else if (ok == 0 && fail == 0) {
                std::cout << "No map files found on robot." << std::endl;
            } else {
                std::cout << "download_maps done: " << ok << " succeeded, " << fail << " failed." << std::endl;
            }

        } else if (cmd == "get_pose") {
            std::string pose_state;
            int32_t ret = slam_client.GetPoseState(pose_state);
            if (ret == 0 || ret == 1) {
                std::cout << "Pose State: " << pose_state << std::endl;
            } else {
                std::cerr << "Command '" << cmd << "' execution failed, error code: " << ret << std::endl;
            }
        } else {
            std::cerr << "Unknown command: " << cmd << std::endl;
            continue;
        }
    }

    return 0;
}

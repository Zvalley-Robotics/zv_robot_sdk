#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <zv/robot/channel/channel_factory.hpp>
#include <zv/robot/media/media_client.hpp>

using namespace zv::robot;

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

bool parseStreamCommand(const std::string & param, std::vector<std::string> & camera_codes, std::string & stream_mode)
{
    std::vector<std::string> values = stringToStringVector(param);
    if (values.size() < 2) {
        std::cerr << "Stream command requires at least one camera code and a mode." << std::endl;
        return false;
    }

    stream_mode = values.back();
    values.pop_back();
    if (stream_mode != "local" && stream_mode != "remote" && stream_mode != "both") {
        std::cerr << "Invalid stream mode: " << stream_mode << ". Use local, remote or both." << std::endl;
        return false;
    }

    camera_codes = std::move(values);
    return true;
}

bool readTextFile(const std::string & path, std::string & content)
{
    std::ifstream input(path);
    if (!input) {
        std::cerr << "Failed to open TRRO config file: " << path << std::endl;
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    content = buffer.str();
    return true;
}

bool printControlResult(const std::string & command, int32_t ret, bool success, const std::string & message)
{
    std::ostream & output = ret == RPC_OK && success ? std::cout : std::cerr;
    output << command << ": rpc_code=" << ret << ", success=" << std::boolalpha << success;
    if (!message.empty()) {
        output << ", message=" << message;
    }
    output << std::endl;
    return ret == RPC_OK && success;
}

int main(int argc, char const * argv[])
{
    std::map<std::string, std::string> args = {
        {"domain_id",         "0"},
        {"network_interface", "" },
        {"trro_config_file",  "" }
    };

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--", 0) != 0) {
            continue;
        }

        size_t pos = arg.find('=');
        std::string key;
        std::string value;
        if (pos != std::string::npos) {
            key = arg.substr(2, pos - 2);
            value = arg.substr(pos + 1);
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }
        } else {
            key = arg.substr(2);
        }
        args[key] = value;
    }

    if (argc == 1 || args.count("help")) {
        std::cout << "Usage: streamer [--domain_id=<id>] [--network_interface=<iface>] "
                     "[--trro_config_file=<path>] <command[=args]>\n"
                  << "\n"
                  << "查询命令:\n"
                  << "  --ping                                  检查 DDS 连接与服务端 API 版本\n"
                  << "\n"
                  << "视频命令:\n"
                  << "  --start_stream=\"<camera...> <mode>\"    开启视频流\n"
                  << "  --stop_stream=\"<camera...> <mode>\"     关闭视频流\n"
                  << "                                            mode: local|remote|both\n"
                  << "                                            camera: head|chest|left|right\n"
                  << "\n"
                  << "音频命令:\n"
                  << "  --start_audio                            请求开启 TRRO 音频流\n"
                  << "  --stop_audio                             请求关闭 TRRO 音频流\n"
                  << "\n"
                  << "示例:\n"
                  << "  streamer --ping\n"
                  << "  streamer --domain_id=1 --network_interface=eth0 --ping\n"
                  << "  streamer --start_stream=\"head local\"\n"
                  << "  streamer --start_stream=\"head chest both\"\n"
                  << "  streamer --stop_stream=\"head local\"\n"
                  << "  streamer --trro_config_file=trro_config.json "
                     "--start_stream=\"head remote\"\n"
                  << "  streamer --start_audio\n"
                  << "  streamer --stop_audio\n";
        return 0;
    }

    std::string trro_config;
    const std::string & trro_config_file = args.at("trro_config_file");
    if (!trro_config_file.empty() && !readTextFile(trro_config_file, trro_config)) {
        return 1;
    }

    int32_t domain_id = 0;
    try {
        size_t parsed_size = 0;
        domain_id = std::stoi(args.at("domain_id"), &parsed_size);
        if (parsed_size != args.at("domain_id").size() || domain_id < 0 || domain_id > 232) {
            throw std::out_of_range("domain ID outside ROS2 range");
        }
    } catch (const std::exception & error) {
        std::cerr << "Invalid domain_id: " << args.at("domain_id") << " (" << error.what() << ")" << std::endl;
        return 1;
    }

    ChannelFactory::Instance()->Init(domain_id, args.at("network_interface"));

    MediaClient client;
    client.Init();
    client.SetTimeout(20.0F);

    int exit_code = 0;
    for (const auto & arg_pair : args) {
        const std::string & command = arg_pair.first;
        const std::string & param = arg_pair.second;
        if (command == "domain_id" || command == "network_interface" || command == "trro_config_file") {
            continue;
        }

        std::cout << "Processing command: [" << command << "] with param: [" << param << "] ..." << std::endl;

        if (command == "ping") {
            PingResult result;
            int32_t ret = client.Ping(result);
            std::ostream & output = ret == RPC_OK ? std::cout : std::cerr;
            output << result.message << std::endl;
            if (ret != RPC_OK) {
                exit_code = 1;
            }
        } else if (command == "start_stream" || command == "stop_stream") {
            std::vector<std::string> camera_codes;
            std::string stream_mode;
            if (!parseStreamCommand(param, camera_codes, stream_mode)) {
                exit_code = 1;
                continue;
            }

            StreamControlResult result;
            int32_t ret = command == "start_stream" ? client.StartStream(camera_codes, stream_mode, result, trro_config)
                                                    : client.StopStream(camera_codes, stream_mode, result);
            if (!printControlResult(command, ret, result.success, result.message)) {
                exit_code = 1;
            }
        } else if (command == "start_audio" || command == "stop_audio") {
            AudioControlResult result;
            int32_t ret = command == "start_audio" ? client.StartAudio(result) : client.StopAudio(result);
            if (!printControlResult(command, ret, result.success, result.message)) {
                exit_code = 1;
            }
            if (command == "start_audio" && ret == RPC_OK && result.success) {
                std::cout << "Audio start was accepted; check streamer logs for "
                             "background audio errors."
                          << std::endl;
            }
        } else {
            std::cerr << "Unknown command: " << command << std::endl;
            exit_code = 1;
        }
    }

    return exit_code;
}

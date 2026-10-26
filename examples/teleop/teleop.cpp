#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <zv/robot/channel/channel_factory.hpp>
#include <zv/robot/teleop/teleop_client.hpp>

using namespace zv::robot;

int main(int argc, char ** argv)
{
    try {
        std::map<std::string, std::string> args = {
            {"domain_id",         "0" },
            {"network_interface", ""  },
            {"timeout",           "30"},
            {"source",            ""  },
            {"task_id",           ""  },
            {"device_id",         ""  },
            {"broker_host",       ""  },
            {"broker_port",       "0" },
            {"product_id",        ""  },
            {"peripheral_type",   ""  },
            {"arm_mode",          ""  },
            {"takeover_mode",     ""  },
            {"header_sec",        "0" },
            {"header_nanosec",    "0" },
            {"reserve",           ""  },
            {"devices_info",      "[]"}
        };
        std::string command;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help") {
                command = "help";
                break;
            }
            if (arg == "--start" || arg == "--stop" || arg == "--ping" || arg == "--get_device_list" ||
                arg == "--switch_devices") {
                if (!command.empty()) throw std::invalid_argument("select exactly one command");
                command = arg.substr(2);
                continue;
            }
            const auto pos = arg.find('=');
            if (arg.rfind("--", 0) != 0 || pos == std::string::npos || !args.count(arg.substr(2, pos - 2))) {
                throw std::invalid_argument("unknown option: " + arg);
            }
            args.at(arg.substr(2, pos - 2)) = arg.substr(pos + 1);
        }
        if (argc == 1 || command == "help") {
            std::cout << "Usage: teleop "
                         "--start|--stop|--ping|--get_device_list|--switch_devices "
                         "[--name=value ...]\n"
                      << "Options: domain_id, network_interface, timeout, source, "
                         "task_id,\n"
                      << "device_id, broker_host, broker_port, product_id, "
                         "peripheral_type, arm_mode,\n"
                      << "takeover_mode, header_sec, header_nanosec, "
                         "reserve, devices_info (JSON array)\n"
                      << "Start/stop require: --source, --task_id.\n"
                      << "trace_id is generated automatically for every call.\n"
                      << "See docs/teleop_interface.md for all service examples.\n";
            return 0;
        }
        if (command.empty())
            throw std::invalid_argument(
                "select --start, --stop, --ping, --get_device_list or "
                "--switch_devices");
        if (command == "start" || command == "stop") {
            for (const char * key : {"source", "task_id"}) {
                if (args.at(key).find_first_not_of(" \t\n\r\f\v") == std::string::npos)
                    throw std::invalid_argument(std::string("--") + key + " is required and must not be blank");
            }
        }
        auto integer = [&](const char * key, int64_t min, int64_t max) {
            size_t end = 0;
            const auto value = std::stoll(args.at(key), &end);
            if (end != args.at(key).size() || value < min || value > max)
                throw std::invalid_argument(std::string("invalid ") + key);
            return value;
        };
        const auto domain = integer("domain_id", 0, 232);
        size_t end = 0;
        const auto timeout = std::stof(args.at("timeout"), &end);
        if (end != args.at("timeout").size() || !std::isfinite(timeout) || timeout <= 0)
            throw std::invalid_argument("timeout must be positive and finite");
        TeleopRequest request;
        request.zv_header.time_().sec_(integer("header_sec", INT32_MIN, INT32_MAX));
        request.zv_header.time_().nanosec_(integer("header_nanosec", 0, 999999999));
        request.zv_header.reserve_(args.at("reserve"));
        request.source = args.at("source");
        request.task_id = args.at("task_id");
        request.device_id = args.at("device_id");
        request.broker_host = args.at("broker_host");
        request.broker_port = integer("broker_port", INT32_MIN, INT32_MAX);
        request.product_id = args.at("product_id");
        request.peripheral_type = args.at("peripheral_type");
        request.arm_mode = args.at("arm_mode");
        request.takeover_mode = args.at("takeover_mode");
        TeleopSwitchDevicesRequest switch_request;
        if (command == "switch_devices") {
            switch_request.zv_header = request.zv_header;
            switch_request.source = request.source;
            const auto devices = nlohmann::json::parse(args.at("devices_info"));
            if (!devices.is_array()) throw std::invalid_argument("devices_info must be a JSON array");
            for (const auto & device : devices)
                switch_request.devices_info.push_back(
                    {device.at("device_type").get<std::string>(), device.at("device_id").get<std::string>()});
        }
        ChannelFactory::Instance()->Init(static_cast<int32_t>(domain), args.at("network_interface"));
        TeleopClient client;
        client.Init();
        client.SetTimeout(timeout);
        if (command == "ping") {
            PingResult result;
            const auto code = client.Ping(result);
            std::cout << "rpc_code=" << code << ", " << result.message << '\n';
            return code == RPC_OK ? 0 : 1;
        }
        if (command == "get_device_list") {
            TeleopDeviceListRequest query;
            query.zv_header = request.zv_header;
            query.source = request.source;
            TeleopDeviceListResult result;
            const auto code = client.GetDeviceList(query, result);
            std::cout << "rpc_code=" << code << ", success=" << std::boolalpha << result.success
                      << ", message=" << result.message << ", source=" << result.source
                      << ", request_trace_id=" << result.request_trace_id
                      << ", response_trace_id=" << result.zv_header.trace_id_()
                      << ", data_source_type=" << unsigned(result.data_source_type)
                      << ", node_name=" << result.status_list.node_name << '\n';
            for (const auto & device : result.status_list.status_list)
                std::cout << "type=" << device.type << ", device_id=" << device.device_id
                          << ", device_no=" << unsigned(device.device_no) << ", is_active=" << device.is_active
                          << ", status_code=" << device.status_code << ", status_message=" << device.status_message
                          << '\n';
            return code == RPC_OK && result.success ? 0 : 1;
        }
        if (command == "switch_devices") {
            TeleopSwitchDevicesResult result;
            const auto code = client.SwitchDevices(switch_request, result);
            std::cout << "rpc_code=" << code << ", success=" << std::boolalpha << result.success
                      << ", message=" << result.message << ", source=" << result.source
                      << ", request_trace_id=" << result.request_trace_id
                      << ", response_trace_id=" << result.zv_header.trace_id_() << '\n';
            return code == RPC_OK && result.success ? 0 : 1;
        }
        TeleopResult result;
        const auto code = command == "start" ? client.Start(request, result) : client.Stop(request, result);
        std::cout << "rpc_code=" << code << ", success=" << std::boolalpha << result.success
                  << ", message=" << result.message << ", source=" << result.source
                  << ", request_trace_id=" << result.request_trace_id << ", response_trace_id=" << result.trace_id
                  << '\n';
        return code == RPC_OK && result.success ? 0 : 1;
    } catch (const std::exception & error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

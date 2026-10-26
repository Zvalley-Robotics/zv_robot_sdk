#pragma once

#include <sys/random.h>

#include <array>
#include <cerrno>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>
#include <zv/idl/t4/ZVHeader_.hpp>
#include <zv/robot/client/client.hpp>
#include <zv/robot/teleop/teleop_api.hpp>
#include <zv/third_party/nlohmann_json/json.hpp>

namespace zv::robot
{

// Caller-supplied ManagerNode fields. Trace IDs are generated per call.
struct TeleopRequest
{
    comm_msgs::msg::dds_::ZVHeader_ zv_header;
    // Required for both Start and Stop; whitespace-only values are invalid.
    std::string source;
    std::string task_id;
    // Optional business fields are forwarded as supplied (empty/zero by default).
    std::string device_id;
    std::string broker_host;
    int32_t broker_port{0};
    std::string product_id;
    std::string peripheral_type;
    std::string arm_mode;
    std::string takeover_mode;
};

struct TeleopResult
{
    // SDK-generated request ID, available even on validation/RPC/parse
    // failure. The remaining fields preserve the ROS service response.
    std::string request_trace_id;
    comm_msgs::msg::dds_::ZVHeader_ zv_header;
    bool success{false};
    std::string message;
    std::string source;
    std::string trace_id;
};

struct TeleopDeviceStatus
{
    std::string type;
    std::string device_id;
    uint8_t device_no{0};
    bool is_active{false};
    uint16_t status_code{0};
    std::string status_message;
};

struct TeleopDeviceStatusList
{
    comm_msgs::msg::dds_::ZVHeader_ zv_header;
    std::string node_name;
    std::vector<TeleopDeviceStatus> status_list;
};

struct TeleopDeviceListRequest
{
    comm_msgs::msg::dds_::ZVHeader_ zv_header;
    std::string source;
};

struct TeleopDeviceListResult
{
    std::string request_trace_id;
    comm_msgs::msg::dds_::ZVHeader_ zv_header;
    bool success{false};
    std::string message;
    uint8_t data_source_type{0};
    TeleopDeviceStatusList status_list;
    std::string source;
};

struct TeleopDeviceSwitch
{
    std::string device_type;
    std::string device_id;
};

struct TeleopSwitchDevicesRequest
{
    comm_msgs::msg::dds_::ZVHeader_ zv_header;
    std::vector<TeleopDeviceSwitch> devices_info;
    std::string source;
};

struct TeleopSwitchDevicesResult
{
    std::string request_trace_id;
    comm_msgs::msg::dds_::ZVHeader_ zv_header;
    bool success{false};
    std::string message;
    std::string source;
};

class TeleopClient : public Client
{
public:
    TeleopClient() : Client(TELEOP_SERVICE_NAME) { SetTimeout(30.0F); }

    void Init() override
    {
        SetApiVersion(TELEOP_API_VERSION);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_TELEOP_START);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_TELEOP_STOP);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_TELEOP_GET_DEVICE_LIST);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_TELEOP_SWITCH_DEVICES);
    }

    int32_t Start(const TeleopRequest & request, TeleopResult & result)
    {
        return Control(ROBOT_API_ID_TELEOP_START, request, result);
    }

    int32_t Stop(const TeleopRequest & request, TeleopResult & result)
    {
        return Control(ROBOT_API_ID_TELEOP_STOP, request, result);
    }

    int32_t GetDeviceList(const TeleopDeviceListRequest & request, TeleopDeviceListResult & result)
    {
        result = TeleopDeviceListResult{};
        try {
            result.request_trace_id = NewTraceId();
        } catch (const std::system_error & error) {
            result.message = error.what();
            return RPC_ERR_UNKNOWN;
        }
        if (request.zv_header.time_().nanosec_() >= 1000000000U) {
            result.message = "zv_header.time.nanosec must be less than 1000000000";
            return RPC_ERR_SERVER_API_PARAMETER;
        }
        const nlohmann::json parameters = {
            {"zv_header", HeaderData(request.zv_header, result.request_trace_id)},
            {"source",           request.source                               }
        };
        std::string data;
        const int32_t code = Call(ROBOT_API_ID_TELEOP_GET_DEVICE_LIST, parameters.dump(), data);
        try {
            const auto response = nlohmann::json::parse(data);
            TeleopDeviceListResult parsed;
            parsed.request_trace_id = result.request_trace_id;
            parsed.zv_header = ReadHeader(response.at("zv_header"));
            parsed.success = response.at("success").get<bool>();
            parsed.message = response.at("message").get<std::string>();
            parsed.data_source_type = ReadInteger(response.at("data_source_type"), 0, UINT8_MAX);
            parsed.source = response.at("source").get<std::string>();
            const auto & list = response.at("status_list");
            parsed.status_list.zv_header = ReadHeader(list.at("zv_header"));
            parsed.status_list.node_name = list.at("node_name").get<std::string>();
            const auto & devices = list.at("status_list");
            if (!devices.is_array()) throw std::invalid_argument("status_list must be an array");
            for (const auto & device : devices) {
                TeleopDeviceStatus status;
                status.type = device.at("type").get<std::string>();
                status.device_id = device.at("device_id").get<std::string>();
                status.device_no = ReadInteger(device.at("device_no"), 0, UINT8_MAX);
                status.is_active = device.at("is_active").get<bool>();
                status.status_code = ReadInteger(device.at("status_code"), 0, UINT16_MAX);
                status.status_message = device.at("status_message").get<std::string>();
                parsed.status_list.status_list.push_back(std::move(status));
            }
            result = std::move(parsed);
        } catch (const nlohmann::json::exception &) {
            return code == RPC_OK ? RPC_ERR_CLIENT_API_DATA : code;
        } catch (const std::invalid_argument &) {
            return code == RPC_OK ? RPC_ERR_CLIENT_API_DATA : code;
        }
        return code;
    }

    int32_t SwitchDevices(const TeleopSwitchDevicesRequest & request, TeleopSwitchDevicesResult & result)
    {
        result = TeleopSwitchDevicesResult{};
        try {
            result.request_trace_id = NewTraceId();
        } catch (const std::system_error & error) {
            result.message = error.what();
            return RPC_ERR_UNKNOWN;
        }
        if (request.zv_header.time_().nanosec_() >= 1000000000U) {
            result.message = "zv_header.time.nanosec must be less than 1000000000";
            return RPC_ERR_SERVER_API_PARAMETER;
        }
        auto devices = nlohmann::json::array();
        for (const auto & device : request.devices_info)
            devices.push_back({
                {"device_type", device.device_type},
                {"device_id",   device.device_id  }
            });
        const nlohmann::json parameters = {
            {"zv_header", HeaderData(request.zv_header, result.request_trace_id)},
            {"devices_info",           devices                         },
            {"source",           request.source      }
        };
        std::string data;
        const int32_t code = Call(ROBOT_API_ID_TELEOP_SWITCH_DEVICES, parameters.dump(), data);
        try {
            const auto response = nlohmann::json::parse(data);
            TeleopSwitchDevicesResult parsed;
            parsed.request_trace_id = result.request_trace_id;
            parsed.zv_header = ReadHeader(response.at("zv_header"));
            parsed.success = response.at("success").get<bool>();
            parsed.message = response.at("message").get<std::string>();
            parsed.source = response.at("source").get<std::string>();
            result = std::move(parsed);
        } catch (const nlohmann::json::exception &) {
            return code == RPC_OK ? RPC_ERR_CLIENT_API_DATA : code;
        } catch (const std::invalid_argument &) {
            return code == RPC_OK ? RPC_ERR_CLIENT_API_DATA : code;
        }
        return code;
    }

private:
    static nlohmann::json HeaderData(const comm_msgs::msg::dds_::ZVHeader_ & header, const std::string & trace_id)
    {
        return {
            {"time",     {{"sec", header.time_().sec_()}, {"nanosec", header.time_().nanosec_()}}},
            {"trace_id", trace_id                                                                },
            {"reserve",  header.reserve_()                                                       }
        };
    }

    static int64_t ReadInteger(const nlohmann::json & value, int64_t min, int64_t max)
    {
        // Compare unsigned JSON numbers before converting: mixed signed
        // JSON comparisons can otherwise wrap UINT64_MAX to -1.
        if (value.is_number_unsigned()) {
            const auto number = value.get<uint64_t>();
            if (max < 0 || number > static_cast<uint64_t>(max) || (min > 0 && number < static_cast<uint64_t>(min)))
                throw std::invalid_argument("integer outside allowed range");
            return static_cast<int64_t>(number);
        }
        if (!value.is_number_integer()) throw std::invalid_argument("expected integer");
        const auto number = value.get<int64_t>();
        if (number < min || number > max) throw std::invalid_argument("integer outside allowed range");
        return number;
    }

    static comm_msgs::msg::dds_::ZVHeader_ ReadHeader(const nlohmann::json & header)
    {
        comm_msgs::msg::dds_::ZVHeader_ result;
        const auto & time = header.at("time");
        result.time_().sec_(ReadInteger(time.at("sec"), INT32_MIN, INT32_MAX));
        result.time_().nanosec_(ReadInteger(time.at("nanosec"), 0, 999999999));
        result.trace_id_(header.at("trace_id").get<std::string>());
        result.reserve_(header.at("reserve").get<std::string>());
        return result;
    }

    static std::string NewTraceId()
    {
        // UUID v4 from the OS random source: independent across processes,
        // clients and threads, without a timestamp or shared PRNG seed.
        std::array<unsigned char, 16> bytes{};
        size_t offset = 0;
        while (offset < bytes.size()) {
            const auto count = ::getrandom(bytes.data() + offset, bytes.size() - offset, 0);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) {
                throw std::system_error(count < 0 ? errno : EIO, std::generic_category(), "generate teleop trace ID");
            }
            offset += static_cast<size_t>(count);
        }
        bytes[6] = (bytes[6] & 0x0f) | 0x40;
        bytes[8] = (bytes[8] & 0x3f) | 0x80;
        constexpr char hex[] = "0123456789abcdef";
        std::string id;
        id.reserve(36);
        for (size_t i = 0; i < bytes.size(); ++i) {
            if (i == 4 || i == 6 || i == 8 || i == 10) id += '-';
            id += hex[bytes[i] >> 4];
            id += hex[bytes[i] & 0x0f];
        }
        return id;
    }

    int32_t Control(int32_t api_id, const TeleopRequest & request, TeleopResult & result)
    {
        result = TeleopResult{};
        try {
            result.request_trace_id = NewTraceId();
        } catch (const std::system_error & error) {
            result.message = error.what();
            return RPC_ERR_UNKNOWN;
        }
        for (const auto & field :
             {std::make_pair("source", &request.source), std::make_pair("task_id", &request.task_id)}) {
            if (field.second->find_first_not_of(" \t\n\r\f\v") == std::string::npos) {
                result.message = std::string(field.first) + " is required and must not be blank";
                return RPC_ERR_SERVER_API_PARAMETER;
            }
        }
        if (request.zv_header.time_().nanosec_() >= 1000000000U) {
            result.message = "zv_header.time.nanosec must be less than 1000000000";
            return RPC_ERR_SERVER_API_PARAMETER;
        }
        const nlohmann::json parameters = {
            {"zv_header", HeaderData(request.zv_header, result.request_trace_id)},
            {"source",           request.source                               },
            {"task_id",           request.task_id},
            {"trace_id",           result.request_trace_id                              },
            {"device_id",           request.device_id},
            {"broker_host",           request.broker_host                            },
            {"broker_port",           request.broker_port},
            {"product_id",           request.product_id                             },
            {"peripheral_type",           request.peripheral_type},
            {"arm_mode",           request.arm_mode                                    },
            {"takeover_mode",           request.takeover_mode}
        };
        std::string data;
        const int32_t code = Call(api_id, parameters.dump(), data);
        try {
            const auto response = nlohmann::json::parse(data);
            TeleopResult parsed;
            parsed.request_trace_id = result.request_trace_id;
            parsed.zv_header = ReadHeader(response.at("zv_header"));
            parsed.success = response.at("success").get<bool>();
            parsed.message = response.at("message").get<std::string>();
            parsed.source = response.at("source").get<std::string>();
            parsed.trace_id = response.at("trace_id").get<std::string>();
            result = std::move(parsed);
        } catch (const nlohmann::json::exception &) {
            return code == RPC_OK ? RPC_ERR_CLIENT_API_DATA : code;
        } catch (const std::invalid_argument &) {
            return code == RPC_OK ? RPC_ERR_CLIENT_API_DATA : code;
        }
        return code;
    }
};

}  // namespace zv::robot

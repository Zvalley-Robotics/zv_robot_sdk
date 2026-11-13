#include <unistd.h>

#include <csignal>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string_view>
#include <zv/idl/t4/BatteryInfo_.hpp>
#include <zv/robot/channel/channel_subscriber.hpp>
#include <zv/third_party/nlohmann_json/json.hpp>

#include "../dds_options.hpp"

using zv::robot::ChannelSubscriber;
using BatteryInfo = robot_interfaces::msg::dds_::BatteryInfo_;
static constexpr const char * BATTERY_TOPIC = "rt/robot/battery/battery_info";
static volatile std::sig_atomic_t STOPPED = 0;
static void stopSubscriber(int) { STOPPED = 1; }

static nlohmann::json batteryJson(const BatteryInfo & battery)
{
    // JSON 保留 SDK 消息的全部原始字段；单位换算只用于展示。
#define BATTERY_FIELD(name) {#name, battery.name##_()}
    return nlohmann::json{
        BATTERY_FIELD(total_volt), BATTERY_FIELD(current), BATTERY_FIELD(soc), BATTERY_FIELD(life),
        BATTERY_FIELD(power), BATTERY_FIELD(energy), BATTERY_FIELD(mos_temp), BATTERY_FIELD(board_temp),
        BATTERY_FIELD(heat_temp), BATTERY_FIELD(heat_current), BATTERY_FIELD(max_volt), BATTERY_FIELD(max_volt_no),
        BATTERY_FIELD(min_volt), BATTERY_FIELD(min_volt_no), BATTERY_FIELD(diff_volt), BATTERY_FIELD(max_temp),
        BATTERY_FIELD(max_temp_no), BATTERY_FIELD(min_temp), BATTERY_FIELD(min_temp_no), BATTERY_FIELD(diff_temp),
        BATTERY_FIELD(chg_mos), BATTERY_FIELD(dis_mos), BATTERY_FIELD(pre_mos), BATTERY_FIELD(heat_mos),
        BATTERY_FIELD(fan_mos), BATTERY_FIELD(bat_state), BATTERY_FIELD(chg_detect), BATTERY_FIELD(load_detect),
        BATTERY_FIELD(do_state), BATTERY_FIELD(di_state), BATTERY_FIELD(cell_num), BATTERY_FIELD(ntc_num),
        BATTERY_FIELD(remain_capacity), BATTERY_FIELD(cycle_count), BATTERY_FIELD(rest_charge_time),
        BATTERY_FIELD(wake_source), BATTERY_FIELD(limit_state), BATTERY_FIELD(limit_current),
        BATTERY_FIELD(soh), BATTERY_FIELD(pwm)};
#undef BATTERY_FIELD
}

static void handler(const void * message, bool json_output)
{
    const auto & battery = *static_cast<const BatteryInfo *>(message);
    if (json_output) {
        std::cout << batteryJson(battery).dump() << std::endl;
        return;
    }
    // 当前 SDK 的温度存储为 uint8，按协议中的 int8 摄氏度解释其原始字节。
    const int temperature = battery.mos_temp_() < 128 ? battery.mos_temp_() : battery.mos_temp_() - 256;
    std::cout << "SOC: " << battery.soc_() / 10.0 << " %, voltage: " << battery.total_volt_() / 10.0
              << " V, current: " << battery.current_() / 10.0 << " A, power: " << battery.power_()
              << " W, MOS temperature: " << temperature << " C, SOH: " << battery.soh_() / 10.0
              << " %, battery state: " << static_cast<int>(battery.bat_state_()) << std::endl;
}

int main(int argc, char ** argv)
{
    std::string network_interface;
    bool use_config = false, json_output = false;
    int period_ms = 2000;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--help") {
            std::cout << "Usage: batterySubscriber [--network_interface=eth0] [--use_config] [--json] "
                         "[--period_ms=2000] (0: every message, max: 60000)\n";
            return 0;
        }
        if (arg == "--json") json_output = true;
        else if (arg == "--use_config") use_config = true;
        else if (arg.substr(0, 12) == "--period_ms=") {
            try {
                period_ms = example::ParseInteger(std::string(arg.substr(12)), 0, 60000);
            } catch (const std::exception & error) {
                std::cerr << error.what() << std::endl;
                return 1;
            }
        }
        else if (arg.substr(0, 20) == "--network_interface=") network_interface = arg.substr(20);
        else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            return 1;
        }
    }
    std::signal(SIGINT, stopSubscriber);
    std::signal(SIGTERM, stopSubscriber);
    try {
        example::InitDds(network_interface, use_config);
        std::mutex output_mutex;
        std::chrono::steady_clock::time_point last_output{};
        ChannelSubscriber<BatteryInfo> subscriber(BATTERY_TOPIC);
        // Best effort 可同时接收 best-effort/reliable 发布端；只保留最近 5 条。
        subscriber.InitChannel([&](const void * message) {
            std::lock_guard<std::mutex> guard(output_mutex);
            const auto now = std::chrono::steady_clock::now();
            if (last_output.time_since_epoch().count() != 0 &&
                now - last_output < std::chrono::milliseconds(period_ms)) return;
            last_output = now;
            handler(message, json_output);
        }, 5, false);
        while (!STOPPED) sleep(1);
        subscriber.CloseChannel();
    } catch (const std::exception & error) {
        std::cerr << "Battery subscriber failed: " << error.what() << std::endl;
        return 1;
    }
    return 0;
}

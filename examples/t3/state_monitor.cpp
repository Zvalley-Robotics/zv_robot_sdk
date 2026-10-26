/****************************************************************
 *
 * t3 状态查询示例：订阅人形机器人（t3）状态话题，缓存每个话题的最新
 * 一帧数据并周期性打印汇总。
 *
 * 命令行参数（均可省略）：
 *   --topics=a,b,c  仅监控指定话题（短名，见下方目录）；缺省监控全部 11 个
 *   --hz=N          打印频率，单位 Hz（默认 1）
 *
 * 话题清单：
 *   rt/left_arm_state    humanoid_msgs/msg/ArmState
 *   rt/left_force        humanoid_msgs/msg/ArmForce
 *   rt/left_gripper      humanoid_msgs/msg/Gripper
 *   rt/left_joint_status humanoid_msgs/msg/JointStatus
 *   rt/right_arm_state   humanoid_msgs/msg/ArmState
 *   rt/right_force       humanoid_msgs/msg/ArmForce
 *   rt/right_gripper     humanoid_msgs/msg/Gripper
 *   rt/right_joint_status humanoid_msgs/msg/JointStatus
 *   rt/head_motor_states motor_driver_interfaces/msg/MotorStates
 *   rt/waist_end         motor_driver_interfaces/msg/WaistEnd
 *   rt/waist_motor_states motor_driver_interfaces/msg/MotorStates
 *
 *****************************************************************/

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <zv/robot/channel/channel_subscriber.hpp>

#include "ArmForce_.hpp"
#include "ArmState_.hpp"
#include "Gripper_.hpp"
#include "JointStatus_.hpp"
#include "MotorStates_.hpp"
#include "WaistEnd_.hpp"

using namespace zv::robot;

// 状态话题（SDK 与 ROS2 互通时话题名需带 rt/ 前缀）
static const std::string LEFT_ARM_STATE_TOPIC = "rt/left_arm_state";
static const std::string LEFT_FORCE_TOPIC = "rt/left_force";
static const std::string LEFT_GRIPPER_TOPIC = "rt/left_gripper";
static const std::string LEFT_JOINT_STATUS_TOPIC = "rt/left_joint_status";
static const std::string RIGHT_ARM_STATE_TOPIC = "rt/right_arm_state";
static const std::string RIGHT_FORCE_TOPIC = "rt/right_force";
static const std::string RIGHT_GRIPPER_TOPIC = "rt/right_gripper";
static const std::string RIGHT_JOINT_STATUS_TOPIC = "rt/right_joint_status";
static const std::string HEAD_MOTOR_STATES_TOPIC = "rt/head_motor_states";
static const std::string WAIST_END_TOPIC = "rt/waist_end";
static const std::string WAIST_MOTOR_STATES_TOPIC = "rt/waist_motor_states";

// 缓存最新一帧消息及接收计数
template <typename MSG>
class LatestMsg
{
public:
    void Store(const void * msg)
    {
        const MSG * real_msg = static_cast<const MSG *>(msg);
        std::lock_guard<std::mutex> lock(mutex_);
        msg_ = *real_msg;
        count_++;
    }

    // 回调线程拷贝最新数据，避免与打印线程竞争
    int64_t Count() const { return count_; }

    MSG Snapshot() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return msg_;
    }

private:
    mutable std::mutex mutex_;
    MSG msg_{};
    int64_t count_ = 0;
};

// 全局缓存
static LatestMsg<humanoid_msgs::msg::dds_::ArmState_> g_left_arm_state;
static LatestMsg<humanoid_msgs::msg::dds_::ArmForce_> g_left_force;
static LatestMsg<humanoid_msgs::msg::dds_::Gripper_> g_left_gripper;
static LatestMsg<humanoid_msgs::msg::dds_::JointStatus_> g_left_joint_status;
static LatestMsg<humanoid_msgs::msg::dds_::ArmState_> g_right_arm_state;
static LatestMsg<humanoid_msgs::msg::dds_::ArmForce_> g_right_force;
static LatestMsg<humanoid_msgs::msg::dds_::Gripper_> g_right_gripper;
static LatestMsg<humanoid_msgs::msg::dds_::JointStatus_> g_right_joint_status;
static LatestMsg<motor_driver_interfaces::msg::dds_::MotorStates_> g_head_motor_states;
static LatestMsg<motor_driver_interfaces::msg::dds_::WaistEnd_> g_waist_end;
static LatestMsg<motor_driver_interfaces::msg::dds_::MotorStates_> g_waist_motor_states;

// 创建订阅者并注册缓存回调
template <typename MSG>
static std::shared_ptr<void> Subscribe(const std::string & topic, LatestMsg<MSG> & cache)
{
    auto subscriber = std::make_shared<ChannelSubscriber<MSG>>(topic);
    subscriber->InitChannel([&cache](const void * msg) { cache.Store(msg); });
    return subscriber;
}

static std::string Join(const std::vector<float> & values, size_t max_count)
{
    std::ostringstream oss;
    for (size_t i = 0; i < values.size() && i < max_count; ++i) {
        if (i > 0) {
            oss << " ";
        }
        oss << values[i];
    }
    return oss.str();
}

template <typename T, size_t N>
static std::string Join(const std::array<T, N> & values)
{
    std::ostringstream oss;
    for (size_t i = 0; i < N; ++i) {
        if (i > 0) {
            oss << " ";
        }
        oss << values[i];
    }
    return oss.str();
}

static void PrintArmState(const std::string & name, const LatestMsg<humanoid_msgs::msg::dds_::ArmState_> & cache)
{
    std::cout << "[" << name << "]";
    if (cache.Count() == 0) {
        std::cout << " 等待数据..." << std::endl;
        return;
    }

    auto msg = cache.Snapshot();
    const auto & position = msg.pose_().position_();
    const auto & euler = msg.euler_();
    std::cout << " count=" << cache.Count() << " | joint: " << Join(msg.joint_(), 7) << " | pos: " << position.x_()
              << " " << position.y_() << " " << position.z_() << " | euler: " << euler.x_() << " " << euler.y_() << " "
              << euler.z_() << " | arm_err=" << msg.arm_err_() << " sys_err=" << static_cast<int>(msg.sys_err_())
              << " dof=" << static_cast<int>(msg.dof_()) << std::endl;
}

static void PrintArmForce(const std::string & name, const LatestMsg<humanoid_msgs::msg::dds_::ArmForce_> & cache)
{
    std::cout << "[" << name << "]";
    if (cache.Count() == 0) {
        std::cout << " 等待数据..." << std::endl;
        return;
    }

    auto msg = cache.Snapshot();
    std::cout << " count=" << cache.Count() << " | force: " << Join(msg.force_(), 6) << std::endl;
}

static void PrintGripper(const std::string & name, const LatestMsg<humanoid_msgs::msg::dds_::Gripper_> & cache)
{
    std::cout << "[" << name << "]";
    if (cache.Count() == 0) {
        std::cout << " 等待数据..." << std::endl;
        return;
    }

    auto msg = cache.Snapshot();
    std::cout << " count=" << cache.Count() << " | eetype=" << static_cast<int>(msg.eetype_())
              << " enable=" << msg.enable_state_() << " status=" << static_cast<int>(msg.status_())
              << " error=" << static_cast<int>(msg.error_()) << " temp=" << static_cast<int>(msg.temperature_());

    // eetype: 0 因时夹爪 / 1 因时手 / 2 知行夹爪 / 3 傲意手 / 4 吸盘
    switch (msg.eetype_()) {
        case 0:
        case 2:
            std::cout << " | 2finger: mode=" << static_cast<int>(msg.gripper_2finger_().mode_())
                      << " force=" << msg.gripper_2finger_().current_force_()
                      << " actpos=" << msg.gripper_2finger_().actpos_();
            break;
        case 4:
            std::cout << " | sucker: voltage=" << static_cast<int>(msg.sucker_().voltage_())
                      << " is_sucking=" << msg.sucker_().is_sucking_();
            break;
        case 1:
        case 3: {
            const auto & hand = msg.hand_();
            std::cout << " | hand: sys_state=" << hand.sys_state_() << " pos[0-3]: " << hand.pos_()[0] << " "
                      << hand.pos_()[1] << " " << hand.pos_()[2] << " " << hand.pos_()[3];
            break;
        }
        default:
            break;
    }
    std::cout << std::endl;
}

static void PrintJointStatus(const std::string & name, const LatestMsg<humanoid_msgs::msg::dds_::JointStatus_> & cache)
{
    std::cout << "[" << name << "]";
    if (cache.Count() == 0) {
        std::cout << " 等待数据..." << std::endl;
        return;
    }

    auto msg = cache.Snapshot();
    std::cout << " count=" << cache.Count() << " | position: " << Join(msg.joint_position_())
              << " | temp: " << Join(msg.joint_temperature_()) << " | en: ";
    for (size_t i = 0; i < msg.joint_en_flag_().size(); ++i) {
        std::cout << (msg.joint_en_flag_()[i] ? 1 : 0);
    }
    std::cout << " | err: " << Join(msg.joint_err_code_()) << " | force: " << Join(msg.force_())
              << " | plus_state=" << msg.plus_state_() << std::endl;
}

static void PrintMotorStates(const std::string & name,
                             const LatestMsg<motor_driver_interfaces::msg::dds_::MotorStates_> & cache)
{
    std::cout << "[" << name << "]";
    if (cache.Count() == 0) {
        std::cout << " 等待数据..." << std::endl;
        return;
    }

    auto msg = cache.Snapshot();
    std::cout << " count=" << cache.Count() << " motor_class=" << static_cast<int>(msg.motor_class_())
              << " motor_num=" << msg.motor_states_().size() << std::endl;

    for (const auto & motor : msg.motor_states_()) {
        std::cout << "    motor_id=" << static_cast<int>(motor.motor_id_()) << " name=" << motor.name_()
                  << " position=" << motor.position_() << " velocity=" << motor.velocity_()
                  << " effort=" << motor.effort_() << " angle=" << motor.angle_() << " enabled=" << motor.enabled_()
                  << " fault=" << motor.fault_() << " error_code=" << motor.error_code_() << std::endl;
    }
}

static void PrintWaistEnd(const LatestMsg<motor_driver_interfaces::msg::dds_::WaistEnd_> & cache)
{
    std::cout << "[waist_end]";
    if (cache.Count() == 0) {
        std::cout << " 等待数据..." << std::endl;
        return;
    }

    auto msg = cache.Snapshot();
    std::cout << " count=" << cache.Count() << " | x=" << msg.x_() << " z=" << msg.z_() << " ry=" << msg.ry_()
              << " rz=" << msg.rz_() << std::endl;
}

// ---------------------------------------------------------------------------
// 监控话题目录：顺序即打印顺序，section 相同的条目需连续分组。
// 短名需与 tools/t3loco_web/server.py 的 MONITOR_TOPIC_CATALOG 保持一致。
// ---------------------------------------------------------------------------
struct TopicEntry
{
    std::string name;                                  // 短名（与 web 端一致）
    std::string section;                               // "手臂" / "头部" / "腰部"
    std::function<std::shared_ptr<void>()> subscribe;  // 订阅并写入对应全局缓存
    std::function<void()> print;                       // 打印最新一帧
};

static const std::vector<TopicEntry> & TopicCatalog()
{
    static const std::vector<TopicEntry> catalog = {
        {"left_arm_state",     "手臂", [] { return Subscribe(LEFT_ARM_STATE_TOPIC, g_left_arm_state); },
         [] { PrintArmState("left_arm_state ", g_left_arm_state); }           },
        {"left_force",         "手臂", [] { return Subscribe(LEFT_FORCE_TOPIC, g_left_force); },
         [] { PrintArmForce("left_force ", g_left_force); }                   },
        {"left_gripper",       "手臂", [] { return Subscribe(LEFT_GRIPPER_TOPIC, g_left_gripper); },
         [] { PrintGripper("left_gripper ", g_left_gripper); }                },
        {"left_joint_status",  "手臂", [] { return Subscribe(LEFT_JOINT_STATUS_TOPIC, g_left_joint_status); },
         [] { PrintJointStatus("left_joint_status ", g_left_joint_status); }  },
        {"right_arm_state",    "手臂", [] { return Subscribe(RIGHT_ARM_STATE_TOPIC, g_right_arm_state); },
         [] { PrintArmState("right_arm_state ", g_right_arm_state); }         },
        {"right_force",        "手臂", [] { return Subscribe(RIGHT_FORCE_TOPIC, g_right_force); },
         [] { PrintArmForce("right_force ", g_right_force); }                 },
        {"right_gripper",      "手臂", [] { return Subscribe(RIGHT_GRIPPER_TOPIC, g_right_gripper); },
         [] { PrintGripper("right_gripper ", g_right_gripper); }              },
        {"right_joint_status", "手臂", [] { return Subscribe(RIGHT_JOINT_STATUS_TOPIC, g_right_joint_status); },
         [] { PrintJointStatus("right_joint_status ", g_right_joint_status); }},
        {"head_motor_states",  "头部", [] { return Subscribe(HEAD_MOTOR_STATES_TOPIC, g_head_motor_states); },
         [] { PrintMotorStates("head_motor_states", g_head_motor_states); }   },
        {"waist_end",          "腰部", [] { return Subscribe(WAIST_END_TOPIC, g_waist_end); },
         [] { PrintWaistEnd(g_waist_end); }                                   },
        {"waist_motor_states", "腰部", [] { return Subscribe(WAIST_MOTOR_STATES_TOPIC, g_waist_motor_states); },
         [] { PrintMotorStates("waist_motor_states", g_waist_motor_states); } },
    };
    return catalog;
}

// ---------------------------------------------------------------------------
// 命令行参数解析
// ---------------------------------------------------------------------------
static std::string Trim(const std::string & value)
{
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

static std::vector<std::string> SplitCsv(const std::string & value)
{
    std::vector<std::string> items;
    std::istringstream iss(value);
    std::string token;
    while (std::getline(iss, token, ',')) {
        token = Trim(token);
        if (!token.empty()) {
            items.push_back(token);
        }
    }
    return items;
}

// 解析 --topics=a,b,c；specified 区分「未指定」与「指定但为空」。
static std::vector<std::string> ParseTopicsArg(int argc, char ** argv, bool & specified)
{
    specified = false;
    static const std::string prefix = "--topics=";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.rfind(prefix, 0) == 0) {
            specified = true;
            return SplitCsv(arg.substr(prefix.size()));
        }
    }
    return {};
}

// 解析 --hz=N；缺省或非法时返回 1.0。
static double ParseHzArg(int argc, char ** argv)
{
    static const std::string prefix = "--hz=";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.rfind(prefix, 0) == 0) {
            const std::string value = Trim(arg.substr(prefix.size()));
            try {
                const double hz = std::stod(value);
                if (std::isfinite(hz) && hz > 0.0) {
                    return hz;
                }
            } catch (const std::exception &) {
            }
            std::cout << "警告: 非法 --hz 取值 '" << value << "'，回退为 1Hz" << std::endl;
            return 1.0;
        }
    }
    return 1.0;
}

int main(int argc, char ** argv)
{
    bool topics_specified = false;
    const std::vector<std::string> requested = ParseTopicsArg(argc, argv, topics_specified);
    const double hz = ParseHzArg(argc, argv);

    const std::vector<TopicEntry> & catalog = TopicCatalog();
    const std::set<std::string> requested_set(requested.begin(), requested.end());

    // 按目录顺序挑选被选话题；缺省（未指定 --topics）监控全部
    std::vector<const TopicEntry *> selected;
    for (const TopicEntry & entry : catalog) {
        if (!topics_specified || requested_set.count(entry.name) > 0) {
            selected.push_back(&entry);
        }
    }

    if (topics_specified) {
        for (const std::string & name : requested_set) {
            const bool known = std::any_of(catalog.begin(), catalog.end(),
                                           [&name](const TopicEntry & entry) { return entry.name == name; });
            if (!known) {
                std::cout << "警告: 未知监控话题 '" << name << "'，已忽略" << std::endl;
            }
        }
    }

    if (selected.empty()) {
        std::cerr << "错误: 没有可监控的话题，退出" << std::endl;
        return 2;
    }

    // 初始化 DDS（domain 0）
    ChannelFactory::Instance()->Init(0);

    // 按所选话题订阅；订阅者需存活到循环结束
    std::vector<std::shared_ptr<void>> subscribers;
    subscribers.reserve(selected.size());
    for (const TopicEntry * entry : selected) {
        subscribers.push_back(entry->subscribe());
    }

    std::cout << "t3 状态监控已启动，订阅 " << selected.size() << " 个话题:";
    for (const TopicEntry * entry : selected) {
        std::cout << " " << entry->name;
    }
    std::cout << "，以 " << hz << " Hz 打印汇总..." << std::endl;

    const std::chrono::duration<double> period(1.0 / hz);
    int64_t tick = 0;
    while (true) {
        std::this_thread::sleep_for(period);
        ++tick;

        std::cout << "\n================ T3 State Monitor tick=" << tick << " ================" << std::endl;
        for (const std::string & section : {"手臂", "头部", "腰部"}) {
            bool printed_header = false;
            for (const TopicEntry * entry : selected) {
                if (entry->section != section) {
                    continue;
                }
                if (!printed_header) {
                    std::cout << "--- " << section << " ---" << std::endl;
                    printed_header = true;
                }
                entry->print();
            }
        }
    }

    return 0;
}

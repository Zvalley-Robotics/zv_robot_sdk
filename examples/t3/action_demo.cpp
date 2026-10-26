/****************************************************************
 *
 * t3 动作控制 CLI：通过 4 个控制话题向人形机器人（t3）下发底盘、
 * 头部、腰部与手臂指令。每次调用只执行一个命令（对标 examples/highcmd/loco.cpp），
 * 由 tools/t3loco_web 的「动作控制」页面按命令逐条调用。
 *
 * 话题清单：
 *   rt/cmd_vel                       geometry_msgs/msg/Twist
 *   rt/remote_control/arm_auto_msg   remote_control_interfaces/msg/ArmAutoMsg
 *   rt/remote_control/head_auto_msg  remote_control_interfaces/msg/HeadAutoMsg
 *   rt/remote_control/waist_auto_msg remote_control_interfaces/msg/WaistAutoMsg
 *
 * 命令（每次只允许一个）：
 *   --cmd_vel="vx vy vyaw"        底盘速度(m/s,rad/s)。非零：20Hz 持续发布，收到
 *                                 SIGINT/SIGTERM 后发零速停止；"0 0 0"：单帧零速。
 *   --head="pitch yaw"            头部绝对模式(度)
 *   --waist="x z ry rz"           腰部相对模式（x/z∈[-1,1]，ry/rz 弧度）
 *   --arm="left|right|both j1..j7" 手臂绝对关节角(弧度)；both 时同一组 7 值下发双臂
 *   --arm_pose="left|right x y z rx ry rz" 手臂目标位姿（x/y/z 米，rx/ry/rz 弧度）
 *   --arm_pose="both 左臂6值 右臂6值" 双臂独立目标，使用各臂当前工作坐标系
 *   --release="all|head|waist|arm|cmd_vel"  释放控制权（is_enable=0；cmd_vel 为零速）
 * 修饰符：
 *   --actuate                     实际下发；缺省为演练（仅打印，不下发）
 *
 * 安全说明：默认为演练（dry-run）模式，只打印将要发送的指令，不下发；
 * 加 --actuate 才真正下发指令，请确保机器人周围安全。
 *
 *****************************************************************/

#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <zv/robot/channel/channel_publisher.hpp>

#include "ArmAutoMsg_.hpp"
#include "HeadAutoMsg_.hpp"
#include "Twist_.hpp"
#include "WaistAutoMsg_.hpp"

using namespace zv::robot;

// 控制话题（SDK 与 ROS2 互通时话题名需带 rt/ 前缀）
static const std::string CMD_VEL_TOPIC = "rt/cmd_vel";
static const std::string ARM_AUTO_MSG_TOPIC = "rt/remote_control/arm_auto_msg";
static const std::string HEAD_AUTO_MSG_TOPIC = "rt/remote_control/head_auto_msg";
static const std::string WAIST_AUTO_MSG_TOPIC = "rt/remote_control/waist_auto_msg";

// 指令发布周期 20Hz
static constexpr auto kPublishPeriod = std::chrono::milliseconds(50);
// 一次性命令的突发帧数：20 帧 ≈ 1s，兼顾 DDS 发现与指令送达
static constexpr int kOneShotFrames = 20;

static bool g_actuate = false;                         // 是否真正下发指令
static volatile std::sig_atomic_t g_keep_running = 1;  // 持续发布循环开关

static void HandleSignal(int) { g_keep_running = 0; }

static std::string Trim(const std::string & value)
{
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

// 去掉 shell 手工传参时可能带上的成对引号
static std::string StripQuotes(const std::string & value)
{
    std::string trimmed = Trim(value);
    if (trimmed.size() >= 2 &&
        ((trimmed.front() == '"' && trimmed.back() == '"') || (trimmed.front() == '\'' && trimmed.back() == '\''))) {
        return trimmed.substr(1, trimmed.size() - 2);
    }
    return trimmed;
}

static const char * ModeLabel() { return g_actuate ? "(实际下发)" : "(演练，不下发)"; }

static void PrintHelp(std::ostream & os)
{
    os << "用法: t3ActionDemo --actuate?  <命令>\n"
          "  --cmd_vel=\"vx vy vyaw\"         "
          "底盘速度，非零持续发布，Ctrl+C/停止后发零速\n"
          "  --head=\"pitch yaw\"             头部绝对模式(度)\n"
          "  --waist=\"x z ry rz\"            腰部相对模式(x/z∈[-1,1]，ry/rz "
          "弧度)\n"
          "  --arm=\"left|right|both j1..j7\"  手臂绝对关节角(弧度)，both "
          "下发双臂\n"
          "  --arm_pose=\"left|right x y z rx ry rz\"  手臂目标位姿(x/y/z "
          "米，rx/ry/rz 弧度)\n"
          "  --arm_pose=\"both 左臂6值 右臂6值\"  "
          "双臂独立目标；使用各臂当前工作坐标系\n"
          "  --release=\"all|head|waist|arm|cmd_vel\"  释放控制权\n"
          "  --actuate                       实际下发；缺省为演练（仅打印）\n";
}

// 解析空白分隔的浮点列表，要求恰好 expected 个
static bool ParseFloats(const std::string & value, size_t expected, std::vector<float> & out, std::string & err)
{
    std::istringstream iss(value);
    std::string token;
    while (iss >> token) {
        double x = 0.0;
        try {
            size_t used = 0;
            x = std::stod(token, &used);
            if (used != token.size()) {
                err = "数值解析失败: " + token;
                return false;
            }
        } catch (const std::invalid_argument &) {
            err = "数值解析失败: " + token;
            return false;
        } catch (const std::out_of_range &) {
            err = "数值超出范围: " + token;
            return false;
        }
        if (!std::isfinite(x) || std::abs(x) > std::numeric_limits<float>::max()) {
            err = "数值必须为有限 float32: " + value;
            return false;
        }
        out.push_back(static_cast<float>(x));
    }
    if (!iss.eof()) {
        err = "数值解析失败: " + value;
        return false;
    }
    if (out.size() != expected) {
        err = "需要 " + std::to_string(expected) + " 个数值，实际 " + std::to_string(out.size());
        return false;
    }
    return true;
}

struct Options
{
    bool actuate = false;
    std::string command;  // 空 / help / cmd_vel / head / waist / arm / release
    // arm_pose 为新增的手臂末端位姿命令。
    std::string value;
};

static bool SetCommand(Options & opt, const std::string & command, const std::string & value, std::string & err)
{
    if (!opt.command.empty()) {
        err = "一次只能执行一个命令";
        return false;
    }
    opt.command = command;
    opt.value = value;
    return true;
}

static bool ParseArguments(int argc, char ** argv, Options & opt, std::string & err)
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            opt.command = "help";
            return true;
        }
        if (arg.rfind("--", 0) != 0) {
            err = "无法识别的参数: " + arg;
            return false;
        }

        const std::string body = arg.substr(2);
        const auto eq = body.find('=');
        const std::string key = (eq == std::string::npos) ? body : body.substr(0, eq);
        const bool has_value = (eq != std::string::npos);
        const std::string value = has_value ? StripQuotes(body.substr(eq + 1)) : "";

        if (key == "actuate") {
            if (has_value) {
                err = "--actuate 不接受取值";
                return false;
            }
            opt.actuate = true;
            continue;
        }
        if (key == "cmd_vel" || key == "head" || key == "waist" || key == "arm" || key == "arm_pose" ||
            key == "release") {
            if (!has_value || value.empty()) {
                err = "--" + key + " 需要取值";
                return false;
            }
            if (!SetCommand(opt, key, value, err)) {
                return false;
            }
            continue;
        }
        err = "未知参数: " + arg;
        return false;
    }
    return true;
}

// 填充 ZVHeader 时间戳与追踪 ID
static void FillZvHeader(comm_msgs::msg::dds_::ZVHeader_ & header)
{
    auto now = std::chrono::system_clock::now();
    auto sec = std::chrono::time_point_cast<std::chrono::seconds>(now);
    header.time_().sec_(static_cast<int32_t>(sec.time_since_epoch().count()));
    header.time_().nanosec_(
        static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - sec).count()));
    header.trace_id_("t3_action_demo");
}

static geometry_msgs::msg::dds_::Twist_ MakeTwist(double linear_x, double linear_y, double angular_z)
{
    geometry_msgs::msg::dds_::Twist_ msg;
    msg.linear_().x_(linear_x);
    msg.linear_().y_(linear_y);
    msg.linear_().z_(0.0);
    msg.angular_().x_(0.0);
    msg.angular_().y_(0.0);
    msg.angular_().z_(angular_z);
    return msg;
}

// 以 20Hz 突发若干帧；演练模式下只等待不 Write
template <typename MSG>
static void PublishBurst(ChannelPublisher<MSG> & publisher, const MSG & msg, int frames = kOneShotFrames)
{
    for (int i = 0; i < frames; ++i) {
        if (g_actuate) {
            publisher.Write(msg);
        }
        std::this_thread::sleep_for(kPublishPeriod);
    }
}

// ---------------------------------------------------------------------------
// 各命令实现
// ---------------------------------------------------------------------------
static int RunCmdVel(double vx, double vy, double vyaw)
{
    ChannelPublisher<geometry_msgs::msg::dds_::Twist_> publisher(CMD_VEL_TOPIC);
    publisher.InitChannel();

    const bool stop = (vx == 0.0 && vy == 0.0 && vyaw == 0.0);
    std::cout << "[cmd_vel] vx=" << vx << " vy=" << vy << " vyaw=" << vyaw << (stop ? " （停止）" : "") << " "
              << ModeLabel() << std::endl;

    const auto msg = MakeTwist(vx, vy, vyaw);
    if (stop) {
        if (g_actuate) {
            publisher.Write(msg);
        }
        std::cout << "[cmd_vel] 已发送零速" << std::endl;
        return 0;
    }

    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    auto next_publish_time = std::chrono::steady_clock::now();
    int64_t tick = 0;
    while (g_keep_running != 0) {
        if (g_actuate) {
            publisher.Write(msg);
        }
        ++tick;
        if (tick % 20 == 0) {
            std::cout << "[cmd_vel] 运行中 " << (tick / 20) << "s" << std::endl;
        }
        next_publish_time += kPublishPeriod;
        std::this_thread::sleep_until(next_publish_time);
    }

    if (g_actuate) {
        publisher.Write(MakeTwist(0.0, 0.0, 0.0));
        std::cout << "[cmd_vel] 停止，已下发零速" << std::endl;
    } else {
        std::cout << "[cmd_vel] 停止（演练，未下发）" << std::endl;
    }
    return 0;
}

static int RunHead(float pitch, float yaw)
{
    ChannelPublisher<remote_control_interfaces::msg::dds_::HeadAutoMsg_> publisher(HEAD_AUTO_MSG_TOPIC);
    publisher.InitChannel();

    remote_control_interfaces::msg::dds_::HeadAutoMsg_ msg;
    msg.is_enable_(1);
    msg.is_absolute_(1);  // 1 绝对模式（pitch/yaw 单位为度）
    msg.do_ready_(0);
    msg.pitch_(pitch);
    msg.yaw_(yaw);

    std::cout << "[head] 绝对 pitch=" << pitch << "° yaw=" << yaw << "° " << ModeLabel() << std::endl;
    PublishBurst(publisher, msg);
    std::cout << "[head] 发送完成" << std::endl;
    return 0;
}

static int RunWaist(const std::vector<float> & v)
{
    ChannelPublisher<remote_control_interfaces::msg::dds_::WaistAutoMsg_> publisher(WAIST_AUTO_MSG_TOPIC);
    publisher.InitChannel();

    remote_control_interfaces::msg::dds_::WaistAutoMsg_ msg;
    msg.is_enable_(1);
    msg.is_absolute_(0);  // 0 相对模式
    msg.do_ready_(0);
    msg.x_(v[0]);
    msg.z_(v[1]);
    msg.ry_(v[2]);
    msg.rz_(v[3]);
    msg.is_joint_(0);

    std::cout << "[waist] 相对 x=" << v[0] << " z=" << v[1] << " ry=" << v[2] << " rz=" << v[3] << " " << ModeLabel()
              << std::endl;
    PublishBurst(publisher, msg);
    std::cout << "[waist] 发送完成" << std::endl;
    return 0;
}

static int RunArm(const std::string & side, const std::vector<float> & joints)
{
    ChannelPublisher<remote_control_interfaces::msg::dds_::ArmAutoMsg_> publisher(ARM_AUTO_MSG_TOPIC);
    publisher.InitChannel();

    remote_control_interfaces::msg::dds_::ArmAutoMsg_ msg;
    FillZvHeader(msg.zv_header_());
    msg.is_enable_(1);
    msg.eetype_({0});                              // 末端类型：0 因时夹爪
    msg.grip_status_({static_cast<uint8_t>(-1)});  // -1 夹爪无动作
    msg.x_(0.0f);
    msg.y_(0.0f);
    msg.z_(0.0f);
    msg.rx_(0.0f);
    msg.ry_(0.0f);
    msg.rz_(0.0f);

    // 1 左臂 / 2 右臂 / 3 双臂；both 时同一组关节角下发两臂
    if (side == "left") {
        msg.arm_index_(1);
        msg.left_joints_(joints);
    } else if (side == "right") {
        msg.arm_index_(2);
        msg.right_joints_(joints);
    } else {
        msg.arm_index_(3);
        msg.left_joints_(joints);
        msg.right_joints_(joints);
    }

    std::cout << "[arm] " << side << " 关节:";
    for (float j : joints) {
        std::cout << " " << j;
    }
    std::cout << " " << ModeLabel() << std::endl;
    PublishBurst(publisher, msg);
    std::cout << "[arm] 发送完成" << std::endl;
    return 0;
}

static int RunArmPose(const std::string & side, const std::vector<float> & values)
{
    remote_control_interfaces::msg::dds_::ArmAutoMsg_ msg;
    FillZvHeader(msg.zv_header_());
    msg.is_enable_(1);
    msg.arm_index_(side == "left" ? 1 : (side == "right" ? 2 : 3));
    const size_t arm_count = side == "both" ? 2 : 1;
    msg.eetype_(std::vector<uint8_t>(arm_count, 0));
    msg.grip_status_(std::vector<uint8_t>(arm_count, static_cast<uint8_t>(-1)));
    // 仅填写目标位姿；关节数组保持为空，方向增量字段保持为零。
    // 各臂当前工作坐标系下的 x/y/z（米）、rx/ry/rz（弧度）。
    if (side == "left" || side == "both") {
        msg.left_pose_(std::vector<float>(values.begin(), values.begin() + 6));
    }
    if (side == "right" || side == "both") {
        const auto begin = values.begin() + (side == "both" ? 6 : 0);
        msg.right_pose_(std::vector<float>(begin, begin + 6));
    }

    std::cout << "[arm_pose] " << side << " 目标位姿(米/弧度):";
    for (float value : values) {
        std::cout << " " << value;
    }
    std::cout << " " << ModeLabel() << std::endl;
    if (g_actuate) {
        ChannelPublisher<remote_control_interfaces::msg::dds_::ArmAutoMsg_> publisher(ARM_AUTO_MSG_TOPIC);
        publisher.InitChannel();
        PublishBurst(publisher, msg);
    }
    return 0;
}

static int RunRelease(const std::string & target)
{
    if (target == "cmd_vel" || target == "all") {
        ChannelPublisher<geometry_msgs::msg::dds_::Twist_> publisher(CMD_VEL_TOPIC);
        publisher.InitChannel();
        if (g_actuate) {
            publisher.Write(MakeTwist(0.0, 0.0, 0.0));
        }
        std::cout << "[release] cmd_vel 零速 " << ModeLabel() << std::endl;
    }

    if (target == "head" || target == "all") {
        ChannelPublisher<remote_control_interfaces::msg::dds_::HeadAutoMsg_> publisher(HEAD_AUTO_MSG_TOPIC);
        publisher.InitChannel();
        remote_control_interfaces::msg::dds_::HeadAutoMsg_ msg;
        msg.is_enable_(0);
        PublishBurst(publisher, msg);
        std::cout << "[release] head " << ModeLabel() << std::endl;
    }

    if (target == "waist" || target == "all") {
        ChannelPublisher<remote_control_interfaces::msg::dds_::WaistAutoMsg_> publisher(WAIST_AUTO_MSG_TOPIC);
        publisher.InitChannel();
        remote_control_interfaces::msg::dds_::WaistAutoMsg_ msg;
        msg.is_enable_(0);
        PublishBurst(publisher, msg);
        std::cout << "[release] waist " << ModeLabel() << std::endl;
    }

    if (target == "arm" || target == "all") {
        ChannelPublisher<remote_control_interfaces::msg::dds_::ArmAutoMsg_> publisher(ARM_AUTO_MSG_TOPIC);
        publisher.InitChannel();
        remote_control_interfaces::msg::dds_::ArmAutoMsg_ msg;
        FillZvHeader(msg.zv_header_());
        msg.is_enable_(0);  // 释放手臂控制权
        msg.arm_index_(0);
        PublishBurst(publisher, msg);
        std::cout << "[release] arm " << ModeLabel() << std::endl;
    }

    std::cout << "[release] " << target << " 完成" << std::endl;
    return 0;
}

int main(int argc, char ** argv)
{
    Options opt;
    std::string err;
    if (!ParseArguments(argc, argv, opt, err)) {
        std::cerr << "参数错误: " << err << "\n\n";
        PrintHelp(std::cerr);
        return 1;
    }
    if (opt.command.empty() || opt.command == "help") {
        PrintHelp(std::cout);
        return 0;
    }

    g_actuate = opt.actuate;

    // 参数预解析：在初始化 DDS 之前完成全部校验
    std::vector<float> nums;
    std::string arm_side;
    if (opt.command == "cmd_vel" || opt.command == "head" || opt.command == "waist") {
        const size_t expected = (opt.command == "cmd_vel") ? 3 : ((opt.command == "head") ? 2 : 4);
        if (!ParseFloats(opt.value, expected, nums, err)) {
            std::cerr << "参数错误: " << err << std::endl;
            return 1;
        }
    } else if (opt.command == "arm" || opt.command == "arm_pose") {
        const auto space = opt.value.find_first_of(" \t");
        if (space == std::string::npos) {
            std::cerr << "参数错误: --" << opt.command << " 需要侧别和数值列表" << std::endl;
            return 1;
        }
        arm_side = Trim(opt.value.substr(0, space));
        if (arm_side != "left" && arm_side != "right" && arm_side != "both") {
            std::cerr << "参数错误: --" << opt.command << " 侧别需为 left/right/both" << std::endl;
            return 1;
        }
        const size_t expected = opt.command == "arm" ? 7 : (arm_side == "both" ? 12 : 6);
        if (!ParseFloats(opt.value.substr(space + 1), expected, nums, err)) {
            std::cerr << "参数错误: " << err << std::endl;
            return 1;
        }
    } else if (opt.command == "release") {
        const std::string target = Trim(opt.value);
        if (target != "all" && target != "head" && target != "waist" && target != "arm" && target != "cmd_vel") {
            std::cerr << "参数错误: --release 需为 all/head/waist/arm/cmd_vel" << std::endl;
            return 1;
        }
        opt.value = target;
    }

    if (!g_actuate) {
        std::cout << "演练模式：仅打印指令，不实际下发。加 --actuate "
                     "参数才会真正下发指令！"
                  << std::endl;
    }

    // 初始化 DDS（domain 0）
    if (opt.command != "arm_pose" || g_actuate) {
        ChannelFactory::Instance()->Init(0);
    }

    if (opt.command == "cmd_vel") {
        return RunCmdVel(nums[0], nums[1], nums[2]);
    }
    if (opt.command == "head") {
        return RunHead(nums[0], nums[1]);
    }
    if (opt.command == "waist") {
        return RunWaist(nums);
    }
    if (opt.command == "arm") {
        return RunArm(arm_side, nums);
    }
    if (opt.command == "arm_pose") {
        return RunArmPose(arm_side, nums);
    }
    if (opt.command == "release") {
        return RunRelease(opt.value);
    }

    std::cerr << "参数错误: 未知命令 " << opt.command << std::endl;
    return 1;
}

#pragma once

#include <cstdint>
#include <string>

namespace zv::robot
{

inline const std::string TELEOP_SERVICE_NAME = "teleop";
inline const std::string TELEOP_API_VERSION = "1.0.0";
inline constexpr int32_t ROBOT_API_ID_TELEOP_START = 2001;
inline constexpr int32_t ROBOT_API_ID_TELEOP_STOP = 2002;
inline constexpr int32_t ROBOT_API_ID_TELEOP_GET_DEVICE_LIST = 2003;
inline constexpr int32_t ROBOT_API_ID_TELEOP_SWITCH_DEVICES = 2004;
inline const std::string TELEOP_START_SERVICE = "/teleop/start";
inline const std::string TELEOP_STOP_SERVICE = "/teleop/stop";
inline const std::string TELEOP_GET_DEVICE_LIST_SERVICE = "/teleop/data_loader/get_device_list";
inline const std::string TELEOP_SWITCH_DEVICES_SERVICE = "/teleop/data_loader/switch_devices";

}  // namespace zv::robot

#pragma once

#include <cstdint>
#include <string>

namespace zv::robot
{

inline const std::string LOCO_SERVICE_NAME = "sport";
inline const std::string LOCO_API_VERSION = "1.2.21";

inline constexpr int32_t ROBOT_API_ID_LOCO_ROBOT_STATE = 7001;
inline constexpr int32_t ROBOT_API_ID_LOCO_SUPPORTED_MODELS = 7002;
inline constexpr int32_t ROBOT_API_ID_LOCO_SET_STATE = 7003;
inline constexpr int32_t ROBOT_API_ID_LOCO_SET_MODEL = 7004;
inline constexpr int32_t ROBOT_API_ID_LOCO_MOVE = 7005;
inline constexpr int32_t ROBOT_API_ID_LOCO_RESET_FAULT = 7006;
inline constexpr int32_t ROBOT_API_ID_LOCO_SET_CONFIG = 7007;
inline constexpr int32_t ROBOT_API_ID_LOCO_HEAD_CONTROL = 7008;
inline constexpr int32_t ROBOT_API_ID_LOCO_HEAD_POSE = 7009;
inline constexpr int32_t ROBOT_API_ID_LOCO_VERSION = 7010;

}  // namespace zv::robot

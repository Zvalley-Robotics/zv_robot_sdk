#pragma once

#include <cstdint>
#include <string>

namespace zv::robot
{

const std::string SLAM_SERVICE_NAME = "slam";
const std::string SLAM_API_VERSION = "1.0.0";

const int32_t ROBOT_API_ID_SLAM_START_MAPPING = 1801;
const int32_t ROBOT_API_ID_SLAM_STOP_MAPPING = 1802;
const int32_t ROBOT_API_ID_SLAM_GET_MAP = 1803;
const int32_t ROBOT_API_ID_SLAM_INIT_POSE = 1804;
const int32_t ROBOT_API_ID_SLAM_GET_POSE_STATE = 1805;
const int32_t ROBOT_API_ID_SLAM_POSE_NAV = 1806;
const int32_t ROBOT_API_ID_SLAM_PAUSE_NAV = 1807;
const int32_t ROBOT_API_ID_SLAM_RESUME_NAV = 1808;
const int32_t ROBOT_API_ID_SLAM_CANCEL_NAV = 1809;
const int32_t ROBOT_API_ID_SLAM_CLOSE = 1810;
const int32_t ROBOT_API_ID_SLAM_QUERY_MAP = 1811;
const int32_t ROBOT_API_ID_SLAM_PREVIEW_MAP = 1812;
const int32_t ROBOT_API_ID_SLAM_DELETE_MAP = 1813;
const int32_t ROBOT_API_ID_SLAM_GET_NAV_STATE = 1814;
const int32_t ROBOT_API_ID_SLAM_GET_STATE = 1815;
const int32_t ROBOT_API_ID_SLAM_RESTART_ODOM = 1816;
const int32_t ROBOT_API_ID_SLAM_TERMINATE_MAPPING = 1822;

}  // namespace zv::robot

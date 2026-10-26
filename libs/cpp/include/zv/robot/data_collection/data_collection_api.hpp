#pragma once

#include <cstdint>
#include <string>

namespace zv::robot
{

inline const std::string DATA_COLLECTION_SERVICE_NAME = "data_collection";
inline const std::string DATA_COLLECTION_API_VERSION = "1.0.0";

inline constexpr int32_t ROBOT_API_ID_DATA_COLLECTION_START = 1901;
inline constexpr int32_t ROBOT_API_ID_DATA_COLLECTION_STOP = 1902;
inline constexpr int32_t ROBOT_API_ID_DATA_COLLECTION_GET_TOPICS = 1903;
inline constexpr int32_t ROBOT_API_ID_DATA_COLLECTION_SET_TOPICS = 1904;

}  // namespace zv::robot

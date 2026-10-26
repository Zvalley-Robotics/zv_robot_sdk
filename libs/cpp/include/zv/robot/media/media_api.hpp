#pragma once

#include <cstdint>
#include <string>

namespace zv::robot
{

const std::string MEDIA_SERVICE_NAME = "media";
const std::string MEDIA_API_VERSION = "1.0.0";

const int32_t ROBOT_API_ID_MEDIA_STREAM_CONTROL = 8001;
const int32_t ROBOT_API_ID_MEDIA_START_AUDIO = 8002;
const int32_t ROBOT_API_ID_MEDIA_STOP_AUDIO = 8003;

}  // namespace zv::robot

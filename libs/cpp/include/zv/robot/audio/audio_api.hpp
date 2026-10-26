#pragma once

#include <cstdint>
#include <string>

namespace zv::robot
{

// DDS-RPC service exposed by the robot-side audio_fronted bridge.
const std::string AUDIO_SERVICE_NAME = "audio";
const std::string AUDIO_API_VERSION = "1.0.0";

// RPC operations mapped one-to-one to audio_fronted ROS 2 services.
const int32_t ROBOT_API_ID_AUDIO_SYNTHESIZE_SPEECH = 6001;
const int32_t ROBOT_API_ID_AUDIO_PLAY_WAV_STREAM = 6002;
const int32_t ROBOT_API_ID_AUDIO_PLAY_WAV_FILE = 6003;

// Source ROS 2 service names used by the robot-side bridge.
const std::string AUDIO_SYNTHESIZE_SPEECH_SERVICE = "/speech/tts/synthesize";
const std::string AUDIO_PLAY_WAV_STREAM_SERVICE = "/audio/player/play_wav_stream";
const std::string AUDIO_PLAY_WAV_FILE_SERVICE = "/audio/player/play_wav_file";

// Native DDS topic names for audio_fronted's streaming data plane.
const std::string AUDIO_RAW_TOPIC = "rt/audio/raw";
const std::string AUDIO_SPEECH_STATUS_TOPIC = "rt/speech/status";
const std::string AUDIO_SPEECH_RESULT_TOPIC = "rt/speech/result";
const std::string AUDIO_KWS_TRIGGER_TOPIC = "rt/speech/kws_trigger";
const std::string AUDIO_CAPTURE_DIAGNOSTICS_TOPIC = "rt/audio/capture/diagnostics";
const std::string AUDIO_INFERENCE_DIAGNOSTICS_TOPIC = "rt/voice/inference/diagnostics";

// zv_api bridge outputs. These use the SDK's existing Response_ wire type;
// the native ROS topics above retain their original voice_msgs/std_msgs types.
const std::string AUDIO_SDK_RAW_TOPIC = "rt/rpc/audio/raw";
const std::string AUDIO_SDK_SPEECH_STATUS_TOPIC = "rt/rpc/audio/speech_status";
const std::string AUDIO_SDK_SPEECH_RESULT_TOPIC = "rt/rpc/audio/speech_result";
const std::string AUDIO_SDK_KWS_TRIGGER_TOPIC = "rt/rpc/audio/kws_trigger";
const std::string AUDIO_SDK_CAPTURE_DIAGNOSTICS_TOPIC = "rt/rpc/audio/capture_diagnostics";
const std::string AUDIO_SDK_INFERENCE_DIAGNOSTICS_TOPIC = "rt/rpc/audio/inference_diagnostics";

}  // namespace zv::robot

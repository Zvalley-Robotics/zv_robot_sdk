#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "audio_api.hpp"

namespace zv::robot
{

enum class AudioTopic { Raw = 0, SpeechStatus, SpeechResult, KwsTrigger, CaptureDiagnostics, InferenceDiagnostics };

struct AudioArrayDimension
{
    std::string label;
    uint32_t size{0};
    uint32_t stride{0};
};

struct AudioRawFrame
{
    int64_t sequence{0};
    std::vector<float> samples;
    std::vector<AudioArrayDimension> dimensions;
    uint32_t data_offset{0};
};

struct AudioSpeechStatus
{
    int64_t sequence{0};
    uint8_t status{0};
    std::string wakeword_id;
};

struct AudioSpeechResult
{
    int64_t sequence{0};
    std::string text;
    float confidence{0.0F};
    bool is_final{false};
    int32_t timestamp_sec{0};
    uint32_t timestamp_nanosec{0};
    std::string language;
    std::string asr_mode;
};

struct AudioKwsTrigger
{
    int64_t sequence{0};
    std::string data;
};

struct AudioVoiceDiagnostics
{
    int64_t sequence{0};
    float vad_energy{0.0F};
    float vad_speech_prob{0.0F};
    float kws_confidence{0.0F};
    uint8_t fsm_state{0};
    uint8_t asr_state{0};
    std::string requested_asr_backend;
    std::string active_asr_backend;
    std::string error_msg;
    uint32_t frames_processed{0};
};

struct AudioTopicStats
{
    uint64_t received{0};
    uint64_t invalid{0};
    uint64_t dropped{0};
    size_t pending{0};
    int32_t matched_publishers{0};
};

// Bounded FIFO subscriptions: preserve partial/final ASR and wake events, with
// explicit overflow counters. Init ChannelFactory before Init(); Close before
// releasing ChannelFactory. No user/Python code executes on the DDS listener.
class AudioSubscriber
{
public:
    explicit AudioSubscriber(size_t queue_depth = 100);
    ~AudioSubscriber();
    AudioSubscriber(const AudioSubscriber &) = delete;
    AudioSubscriber & operator=(const AudioSubscriber &) = delete;

    void Init(const std::vector<AudioTopic> & topics = {
                  AudioTopic::Raw, AudioTopic::SpeechStatus, AudioTopic::SpeechResult, AudioTopic::KwsTrigger,
                  AudioTopic::CaptureDiagnostics, AudioTopic::InferenceDiagnostics});
    void Close();

    // Nonblocking: false means the selected topic's queue is empty. On false,
    // output is reset. Init/Close reset the queues and counters.
    bool TakeRawAudio(AudioRawFrame & output);
    bool TakeSpeechStatus(AudioSpeechStatus & output);
    bool TakeSpeechResult(AudioSpeechResult & output);
    bool TakeKwsTrigger(AudioKwsTrigger & output);
    bool TakeCaptureDiagnostics(AudioVoiceDiagnostics & output);
    bool TakeInferenceDiagnostics(AudioVoiceDiagnostics & output);
    AudioTopicStats GetStats(AudioTopic topic) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace zv::robot

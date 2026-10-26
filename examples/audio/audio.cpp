#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <zv/robot/audio/audio_client.hpp>
#include <zv/robot/audio/audio_subscriber.hpp>
#include <zv/robot/channel/channel_factory.hpp>
#include <zv/robot/internal/internal.hpp>

namespace
{

std::map<std::string, std::string> ParseArgs(int argc, const char * const argv[])
{
    std::map<std::string, std::string> args = {
        {"domain_id",         "0"  },
        {"network_interface", ""   },
        {"timeout",           "120"},
        {"output_name",       ""   },
        {"speaker_id",        "-1" },
        {"speaker",           ""   },
        {"duration",          "10" }
    };

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument.rfind("--", 0) != 0) {
            continue;
        }

        const size_t equals = argument.find('=');
        std::string key = argument.substr(2, equals == std::string::npos ? equals : equals - 2);
        std::string value;
        if (equals != std::string::npos) {
            value = argument.substr(equals + 1);
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }
        }
        args[key] = value;
    }
    return args;
}

void PrintUsage()
{
    std::cout << "Usage: audio [connection options] <command[=args]>\n\n"
              << "连接参数:\n"
              << "  --domain_id=<id>                 DDS domain，默认 0\n"
              << "  --network_interface=<iface>      DDS 网卡，默认自动选择\n"
              << "  --timeout=<seconds>              同步调用超时，默认 120 秒\n\n"
              << "命令:\n"
              << "  --ping                           检查 audio RPC bridge\n"
              << "  --synthesize=<text>              合成语音到机器人端 WAV 文件\n"
              << "    [--output_name=<name.wav>] [--speaker_id=<id>] "
                 "[--speaker=<name>]\n"
              << "  --play_wav_stream=<local.wav>    上传 SDK 主机上的完整 WAV 并播放\n"
              << "  --play_wav_file=<robot.wav>      播放机器人主机上的 WAV 路径\n\n"
              << "  --watch=<topic>                 订阅 "
                 "all/raw/status/result/kws/capture/inference\n"
              << "    [--duration=<seconds>]         订阅时长，默认 10 秒\n\n"
              << "示例:\n"
              << "  audio --ping\n"
              << "  audio --synthesize=\"你好，我是小谷。\" --speaker=ryan\n"
              << "  audio --play_wav_stream=/tmp/hello.wav\n"
              << "  audio --play_wav_file=/tmp/audio_fronted_tts/hello.wav\n";
}

bool ReadBinaryFile(const std::string & path, std::vector<uint8_t> & data)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }
    data.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return !input.bad() && !data.empty();
}

int WatchTopics(const std::string & selection, double duration)
{
    using namespace zv::robot;
    const std::map<std::string, AudioTopic> topics{
        {"raw",       AudioTopic::Raw                 },
        {"status",    AudioTopic::SpeechStatus        },
        {"result",    AudioTopic::SpeechResult        },
        {"kws",       AudioTopic::KwsTrigger          },
        {"capture",   AudioTopic::CaptureDiagnostics  },
        {"inference", AudioTopic::InferenceDiagnostics}
    };
    if (selection != "all" && topics.count(selection) == 0) {
        std::cerr << "Unknown audio topic: " << selection << std::endl;
        return 1;
    }
    std::vector<AudioTopic> selected;
    for (const auto & [name, topic] : topics) {
        if (selection == "all" || name == selection) selected.push_back(topic);
    }
    AudioSubscriber subscriber;
    subscriber.Init(selected);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<double>(duration);
    while (std::chrono::steady_clock::now() < deadline) {
        AudioRawFrame raw;
        if (subscriber.TakeRawAudio(raw)) {
            std::cout << "raw: sequence=" << raw.sequence << ", samples=" << raw.samples.size() << '\n';
        }
        AudioSpeechStatus status;
        if (subscriber.TakeSpeechStatus(status)) {
            std::cout << "status: " << static_cast<unsigned>(status.status) << ", wakeword=" << status.wakeword_id
                      << '\n';
        }
        AudioSpeechResult result;
        if (subscriber.TakeSpeechResult(result)) {
            std::cout << "result: " << result.text << ", final=" << std::boolalpha << result.is_final
                      << ", backend=" << result.asr_mode << '\n';
        }
        AudioKwsTrigger kws;
        if (subscriber.TakeKwsTrigger(kws)) std::cout << "kws: " << kws.data << '\n';
        AudioVoiceDiagnostics diagnostic;
        if (subscriber.TakeCaptureDiagnostics(diagnostic)) {
            std::cout << "capture: frames=" << diagnostic.frames_processed << ", error=" << diagnostic.error_msg
                      << '\n';
        }
        if (subscriber.TakeInferenceDiagnostics(diagnostic)) {
            std::cout << "inference: asr_state=" << static_cast<unsigned>(diagnostic.asr_state)
                      << ", backend=" << diagnostic.active_asr_backend << ", error=" << diagnostic.error_msg << '\n';
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    for (const auto & [name, topic] : topics) {
        if (selection != "all" && selection != name) continue;
        const auto stats = subscriber.GetStats(topic);
        std::cout << name << ": publishers=" << stats.matched_publishers << ", received=" << stats.received
                  << ", invalid=" << stats.invalid << ", dropped=" << stats.dropped << ", pending=" << stats.pending
                  << '\n';
    }
    subscriber.Close();
    return 0;
}

bool PrintPlaybackResult(const std::string & operation, int32_t ret, const zv::robot::WavPlaybackResult & result)
{
    std::ostream & output = ret == zv::robot::RPC_OK && result.success ? std::cout : std::cerr;
    output << operation << ": rpc_code=" << ret << ", success=" << std::boolalpha << result.success
           << ", sample_rate=" << result.sample_rate << ", channels=" << result.channels
           << ", bits_per_sample=" << result.bits_per_sample << ", encoding=" << result.encoding
           << ", frames_played=" << result.frames_played << ", duration_s=" << result.duration_s;
    if (!result.error_msg.empty()) {
        output << ", error=" << result.error_msg;
    }
    output << std::endl;
    return ret == zv::robot::RPC_OK && result.success;
}

}  // namespace

int main(int argc, const char * const argv[])
{
    const auto args = ParseArgs(argc, argv);
    if (argc == 1 || args.count("help") != 0) {
        PrintUsage();
        return 0;
    }

    int32_t domain_id = 0;
    float timeout = 0.0F;
    int32_t speaker_id = -1;
    double duration = 10.0;
    try {
        domain_id = std::stoi(args.at("domain_id"));
        timeout = std::stof(args.at("timeout"));
        speaker_id = std::stoi(args.at("speaker_id"));
        duration = std::stod(args.at("duration"));
    } catch (const std::exception & error) {
        std::cerr << "Invalid numeric option: " << error.what() << std::endl;
        return 1;
    }
    if (domain_id < 0 || domain_id > 232 || !std::isfinite(timeout) || timeout <= 0.0F || speaker_id < -1 ||
        !std::isfinite(duration) || duration <= 0) {
        std::cerr << "domain_id must be 0..232, timeout must be positive, and "
                     "speaker_id must be >= -1"
                  << std::endl;
        return 1;
    }

    zv::robot::ChannelFactory::Instance()->Init(domain_id, args.at("network_interface"));
    if (args.count("watch")) return WatchTopics(args.at("watch"), duration);
    zv::robot::AudioClient client;
    client.Init();
    client.SetTimeout(timeout);

    int exit_code = 0;
    for (const auto & [command, value] : args) {
        if (command == "domain_id" || command == "network_interface" || command == "timeout" ||
            command == "output_name" || command == "speaker_id" || command == "speaker" || command == "duration") {
            continue;
        }

        if (command == "ping") {
            zv::robot::PingResult result;
            const int32_t ret = client.Ping(result);
            (ret == zv::robot::RPC_OK ? std::cout : std::cerr) << result.message << std::endl;
            exit_code = ret == zv::robot::RPC_OK ? exit_code : 1;
        } else if (command == "synthesize") {
            zv::robot::SynthesizeSpeechResult result;
            const int32_t ret =
                client.SynthesizeSpeech(value, result, args.at("output_name"), speaker_id, args.at("speaker"));
            std::ostream & output = ret == zv::robot::RPC_OK && result.success ? std::cout : std::cerr;
            output << "synthesize: rpc_code=" << ret << ", success=" << std::boolalpha << result.success
                   << ", file_path=" << result.file_path << ", backend=" << result.backend
                   << ", sample_rate=" << result.sample_rate << ", duration_s=" << result.duration_s
                   << ", rtf=" << result.rtf;
            if (!result.error_msg.empty()) {
                output << ", error=" << result.error_msg;
            }
            output << std::endl;
            if (ret != zv::robot::RPC_OK || !result.success) {
                exit_code = 1;
            }
        } else if (command == "play_wav_stream") {
            std::vector<uint8_t> wav_data;
            if (!ReadBinaryFile(value, wav_data)) {
                std::cerr << "Failed to read non-empty WAV file: " << value << std::endl;
                exit_code = 1;
                continue;
            }
            zv::robot::WavPlaybackResult result;
            const int32_t ret = client.PlayWavStream(wav_data, result);
            if (!PrintPlaybackResult(command, ret, result)) {
                exit_code = 1;
            }
        } else if (command == "play_wav_file") {
            zv::robot::WavPlaybackResult result;
            const int32_t ret = client.PlayWavFile(value, result);
            if (!PrintPlaybackResult(command, ret, result)) {
                exit_code = 1;
            }
        } else {
            std::cerr << "Unknown command: " << command << std::endl;
            exit_code = 1;
        }
    }

    return exit_code;
}

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <zv/robot/channel/channel_factory.hpp>
#include <zv/robot/data_collection/data_collection_client.hpp>
#include <zv/robot/mcap/mcap_reader.hpp>

namespace fs = std::filesystem;

namespace
{

void printUsage()
{
    std::cout << "Usage:\n"
              << "  dataCollection [--network_interface=<iface>] --start\n"
              << "  dataCollection [--network_interface=<iface>] "
                 "--stop[=<local_dir>]\n"
              << "  dataCollection [--network_interface=<iface>] --get_topics\n"
              << "  dataCollection [--network_interface=<iface>] "
                 "--set_topics=<t1,t2,...>\n"
              << "                 [--mode=replace|add|remove] [--persist]\n"
              << "  dataCollection --read=<local_mcap>\n"
              << "\n"
              << "Commands:\n"
              << "  --start                   Start recording on the robot\n"
              << "  --stop[=<local_dir>]      Stop recording and download the "
                 "MCAP file\n"
              << "                            to local_dir (default: current "
                 "directory)\n"
              << "  --get_topics              Print the configured recording "
                 "topics\n"
              << "  --set_topics=<t1,t2,...>  Modify the recording topic list; "
                 "an empty\n"
              << "                            value clears the list when "
                 "--mode=replace\n"
              << "\n"
              << "Options:\n"
              << "  --mode=replace|add|remove How to apply --set_topics "
                 "(default: replace)\n"
              << "  --persist                 Write the result back to the "
                 "config file\n"
              << "  --read=<local_mcap>        Read a local MCAP file without "
                 "connecting to the robot\n"
              << "  --read_after_download     Read the downloaded MCAP file "
                 "(with --stop)\n"
              << "\n"
              << "Topic output format (one field per line, comma separated):\n"
              << "  Recording: 0|1\n"
              << "  Topics: <configured topics>\n"
              << "  Subscribed: <topics actually subscribed>\n"
              << "  Skipped: <configured but not subscribed>\n"
              << "  Invalid: <entries rejected by the robot>\n";
}

bool parseArguments(int argc, const char * const argv[], std::map<std::string, std::string> & arguments)
{
    arguments["network_interface"] = "lo";
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument.rfind("--", 0) != 0) {
            std::cerr << "Invalid argument: " << argument << std::endl;
            return false;
        }

        const size_t separator = argument.find('=');
        const std::string key = argument.substr(2, separator == std::string::npos ? std::string::npos : separator - 2);
        const std::string value = separator == std::string::npos ? "" : argument.substr(separator + 1);
        if (key != "network_interface" && key != "start" && key != "stop" && key != "get_topics" &&
            key != "set_topics" && key != "mode" && key != "persist" && key != "help" && key != "read" &&
            key != "read_after_download") {
            std::cerr << "Unknown option: --" << key << std::endl;
            return false;
        }
        arguments[key] = value;
    }
    return true;
}

// 话题名不含逗号，因此命令行用逗号分隔多个话题；空值表示空列表
std::vector<std::string> splitTopics(const std::string & value)
{
    std::vector<std::string> topics;
    size_t start = 0;
    while (start <= value.size()) {
        const size_t separator = value.find(',', start);
        const std::string topic =
            value.substr(start, separator == std::string::npos ? std::string::npos : separator - start);
        if (!topic.empty()) topics.push_back(topic);
        if (separator == std::string::npos) break;
        start = separator + 1;
    }
    return topics;
}

bool parseMode(const std::string & value, zv::robot::TopicEditMode & mode)
{
    if (value == "replace") {
        mode = zv::robot::TopicEditMode::kReplace;
        return true;
    }
    if (value == "add") {
        mode = zv::robot::TopicEditMode::kAdd;
        return true;
    }
    if (value == "remove") {
        mode = zv::robot::TopicEditMode::kRemove;
        return true;
    }
    return false;
}

// 输出 "标签: 话题1,话题2"，空列表只输出 "标签:"，便于调用方按行解析
void printTopicList(const char * label, const std::vector<std::string> & topics)
{
    std::cout << label << ":";
    for (size_t index = 0; index < topics.size(); ++index) {
        std::cout << (index == 0 ? " " : ",") << topics[index];
    }
    std::cout << std::endl;
}

// 顺序统计所有消息；预览前 10 条及每个话题首条。
int ReadMcapFile(const std::string & path, std::ostream & output, std::ostream & errors)
{
    zv::robot::McapReader reader;
    if (!reader.Open(path)) {
        errors << "Failed to open MCAP '" << path << "': " << reader.LastError() << '\n';
        return 1;
    }

    output << "MCAP: " << path
           << "\nPreview: first 10 messages and first message per topic, "
              "up to 16 payload bytes\n";
    uint64_t count = 0;
    std::map<std::string, uint64_t> topic_counts;
    // 不依赖 Summary/索引，兼容未写入 Summary 的文件。
    zv::robot::McapMessage message;
    while (true) {
        const auto result = reader.ReadNext(message);
        if (result == zv::robot::McapReadResult::kEndOfFile) break;
        if (result == zv::robot::McapReadResult::kError) {
            errors << "Failed to read MCAP: " << reader.LastError() << '\n';
            return 1;
        }
        ++count;
        const bool first_for_topic = ++topic_counts[message.topic] == 1;
        if (count > 10 && !first_for_topic) continue;

        output << "Message " << count << ": topic=" << message.topic << " encoding=" << message.message_encoding
               << " schema=" << (message.has_schema ? message.schema_name : "<none>")
               << " schema_encoding=" << (message.has_schema ? message.schema_encoding : "<none>")
               << " sequence=" << message.sequence << " log_time_ns=" << message.log_time
               << " publish_time_ns=" << message.publish_time << " bytes=" << message.data.size() << '\n';

        // message 持有自己的 payload，可按 schema/encoding 进一步反序列化。
        std::ostringstream hex;
        hex << std::hex << std::setfill('0');
        for (size_t i = 0; i < std::min<size_t>(message.data.size(), 16); ++i) {
            if (i != 0) hex << ' ';
            hex << std::setw(2) << static_cast<unsigned int>(message.data[i]);
        }
        output << "  payload_hex: " << hex.str() << (message.data.size() > 16 ? " ..." : "") << '\n';
        output << "  business_json: " << message.DecodeJson() << '\n';
    }
    reader.Close();

    output << "Messages: " << count << '\n';
    for (const auto & entry : topic_counts) {
        output << "Topic: " << entry.first << " messages=" << entry.second << '\n';
    }
    return 0;
}

// 统一失败输出：附上服务端返回的错误描述，tools 的 web 界面直接展示 stderr。
void printFailure(const char * action, int32_t code, const zv::robot::DataCollectionClient & client)
{
    std::cerr << action << " (code=" << code;
    const std::string message = client.GetLastErrorMessage();
    if (!message.empty()) {
        std::cerr << ", " << message;
    }
    std::cerr << ")" << std::endl;
}

}  // namespace

int main(int argc, const char * const argv[])
{
    std::map<std::string, std::string> arguments;
    if (!parseArguments(argc, argv, arguments) || argc == 1 || arguments.count("help") != 0) {
        printUsage();
        return argc == 1 || arguments.count("help") != 0 ? 0 : 1;
    }

    const bool start = arguments.count("start") != 0;
    const bool stop = arguments.count("stop") != 0;
    const bool get_topics = arguments.count("get_topics") != 0;
    const bool set_topics = arguments.count("set_topics") != 0;
    const bool read = arguments.count("read") != 0;
    const bool read_after_download = arguments.count("read_after_download") != 0;
    if (static_cast<int>(start) + static_cast<int>(stop) + static_cast<int>(get_topics) + static_cast<int>(set_topics) +
            static_cast<int>(read) !=
        1) {
        std::cerr << "Specify exactly one of --start, --stop, --get_topics or "
                     "--set_topics or --read."
                  << std::endl;
        printUsage();
        return 1;
    }
    // --mode / --persist 只对 --set_topics 生效，显式报错避免用户误以为已生效
    if (!set_topics && (arguments.count("mode") != 0 || arguments.count("persist") != 0)) {
        std::cerr << "--mode/--persist only apply to --set_topics." << std::endl;
        return 1;
    }

    if (read_after_download && !stop) {
        std::cerr << "--read_after_download only applies to --stop." << std::endl;
        return 1;
    }
    if (read && arguments.at("read").empty()) {
        std::cerr << "--read requires a local MCAP file path." << std::endl;
        return 1;
    }
    if (read) return ReadMcapFile(arguments.at("read"), std::cout, std::cerr);

    zv::robot::ChannelFactory::Instance()->Init(0, arguments.at("network_interface"));

    zv::robot::DataCollectionClient client;
    client.Init();
    client.SetTimeout(30.0f);

    if (get_topics) {
        zv::robot::TopicList topics;
        const int32_t result = client.GetRecordingTopics(topics);
        if (result != 0) {
            printFailure("Failed to get recording topics", result, client);
            return 1;
        }
        std::cout << "Recording: " << (topics.recording ? 1 : 0) << std::endl;
        printTopicList("Topics", topics.topics);
        printTopicList("Subscribed", topics.subscribed_topics);
        printTopicList("Skipped", topics.skipped_topics);
        return 0;
    }

    if (set_topics) {
        zv::robot::TopicEditMode mode = zv::robot::TopicEditMode::kReplace;
        const std::string mode_value = arguments.count("mode") != 0 ? arguments.at("mode") : std::string("replace");
        if (!parseMode(mode_value, mode)) {
            std::cerr << "Invalid --mode '" << mode_value << "'; use replace, add or remove." << std::endl;
            return 1;
        }

        zv::robot::TopicUpdateResult topic_result;
        const int32_t result = client.SetRecordingTopics(splitTopics(arguments.at("set_topics")), mode,
                                                         arguments.count("persist") != 0, topic_result);
        if (result != 0) {
            printFailure("Failed to set recording topics", result, client);
            return 1;
        }
        printTopicList("Topics", topic_result.topics);
        printTopicList("Invalid", topic_result.invalid_topics);
        return 0;
    }

    if (start) {
        const int32_t result = client.StartRecording();
        if (result != 0) {
            printFailure("Failed to start recording", result, client);
            return 1;
        }
        std::cout << "Recording started." << std::endl;
        return 0;
    }

    zv::robot::RecordingInfo recording;
    int32_t result = client.StopRecording(recording);
    if (result != 0) {
        printFailure("Failed to stop recording", result, client);
        return 1;
    }

    const fs::path local_dir = arguments.at("stop").empty() ? fs::current_path() : fs::path(arguments.at("stop"));
    std::error_code error;
    fs::create_directories(local_dir, error);
    if (error) {
        std::cerr << "Cannot create download directory '" << local_dir.string() << "': " << error.message()
                  << std::endl;
        return 1;
    }

    const fs::path local_path = local_dir / recording.file.name;
    const auto progress = [&recording](int64_t downloaded, int64_t total) {
        const int64_t expected_total = total > 0 ? total : recording.file.size;
        if (expected_total <= 0) return;
        const int percentage =
            static_cast<int>(static_cast<double>(downloaded) * 100.0 / static_cast<double>(expected_total));
        std::cout << "\rDownloading " << recording.file.name << ": " << percentage << "%" << std::flush;
    };

    result = client.DownloadRecording(recording, local_path.string(), progress);
    if (result != 0) {
        std::cout << std::endl;
        std::cerr << "Failed to download recording (code=" << result << ")" << std::endl;
        return 1;
    }

    std::cout << "\rDownloaded " << recording.file.name << " to " << local_path.string() << " (" << recording.file.size
              << " bytes, " << recording.message_count << " messages)" << std::endl;
    if (read_after_download) {
        return ReadMcapFile(local_path.string(), std::cout, std::cerr);
    }
    return 0;
}

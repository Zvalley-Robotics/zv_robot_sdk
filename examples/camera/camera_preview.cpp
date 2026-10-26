#include <chrono>
#include <cmath>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <stdexcept>
#include <thread>
#include <zv/robot/media/camera_subscriber.hpp>

namespace
{
volatile std::sig_atomic_t running = 1;
void stop(int) { running = 0; }
}  // namespace

int main(int argc, char ** argv)
{
    try {
        int domain = 0;
        std::string interface, output_dir;
        double duration = 0;
        std::map<std::string, std::string> topics;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--help") {
                std::cout << "Usage: cameraPreview [--domain_id=0] "
                             "[--network_interface=eth0]\n"
                             "  "
                             "[--camera=head=rt/media/camera/head/color/"
                             "image_raw/compressed] ...\n"
                             "  [--output_dir=./frames] [--duration=0]\n"
                             "Poll latest compressed frames; optionally save "
                             "<camera>.jpg.\n"
                             "Ctrl-C stops only these subscriptions. No "
                             "camera/recording control.\n";
                return 0;
            }
            const auto pos = arg.find('=');
            if (pos == std::string::npos) throw std::invalid_argument("expected --option=value");
            const auto key = arg.substr(0, pos), value = arg.substr(pos + 1);
            if (key == "--domain_id") {
                size_t parsed = 0;
                domain = std::stoi(value, &parsed);
                if (parsed != value.size()) throw std::invalid_argument("invalid domain ID");
            } else if (key == "--network_interface")
                interface = value;
            else if (key == "--output_dir")
                output_dir = value;
            else if (key == "--duration") {
                size_t parsed = 0;
                duration = std::stod(value, &parsed);
                if (parsed != value.size()) throw std::invalid_argument("invalid duration");
            } else if (key == "--camera") {
                const auto split = value.find('=');
                const auto name = value.substr(0, split);
                if (split == std::string::npos || !std::regex_match(name, std::regex("[A-Za-z0-9_-]+")) ||
                    !topics.emplace(name, value.substr(split + 1)).second)
                    throw std::invalid_argument("expected unique --camera=NAME=DDS_TOPIC");
            } else
                throw std::invalid_argument("unknown option: " + key);
        }
        if (!std::isfinite(duration) || duration < 0) throw std::invalid_argument("invalid duration");
        if (topics.empty())
            for (const auto * name : {"head", "left", "right", "chest"})
                topics[name] = std::string("rt/media/camera/") + name + "/color/image_raw/compressed";
        zv::robot::CameraSubscriber subscriber;
        subscriber.Init(domain, interface, topics);
        if (!output_dir.empty()) std::filesystem::create_directories(output_dir);
        std::signal(SIGINT, stop);
        std::signal(SIGTERM, stop);
        std::map<std::string, uint64_t> counts;
        const auto started = std::chrono::steady_clock::now();
        auto logged = started - std::chrono::seconds(1);
        while (running) {
            const auto now = std::chrono::steady_clock::now();
            if (duration > 0 && std::chrono::duration<double>(now - started).count() >= duration) break;
            for (const auto & [name, topic] : topics) {
                zv::robot::CameraFrame frame;
                if (subscriber.TakeFrame(name, frame)) {
                    ++counts[name];
                    if (!output_dir.empty() && frame.data.size() >= 4 && frame.data[0] == 0xff &&
                        frame.data[1] == 0xd8 && frame.data[frame.data.size() - 2] == 0xff &&
                        frame.data.back() == 0xd9) {
                        const auto path = std::filesystem::path(output_dir) / (name + ".jpg");
                        const auto temp = path.string() + ".tmp";
                        std::ofstream output(temp, std::ios::binary);
                        output.exceptions(std::ios::failbit | std::ios::badbit);
                        output.write(reinterpret_cast<const char *>(frame.data.data()), frame.data.size());
                        output.close();
                        std::filesystem::rename(temp, path);
                    }
                }
                if (now - logged >= std::chrono::seconds(1))
                    std::cout << name << " publishers=" << subscriber.GetPublisherCount(name)
                              << " frames=" << counts[name] << " topic=" << topic << std::endl;
            }
            if (now - logged >= std::chrono::seconds(1)) logged = now;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        subscriber.Close();
        return 0;
    } catch (const std::exception & error) {
        std::cerr << "camera preview: " << error.what() << std::endl;
        return 1;
    }
}

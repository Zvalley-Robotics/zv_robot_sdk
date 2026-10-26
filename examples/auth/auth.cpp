#include <cstdint>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <zv/common/auth_client.hpp>
#include <zv/robot/channel/channel_factory.hpp>
#include <zv/robot/internal/internal.hpp>

using namespace zv::robot;

namespace
{

std::vector<std::string> SplitWords(const std::string & value)
{
    std::istringstream input(value);
    std::vector<std::string> words;
    std::string word;
    while (input >> word) {
        words.push_back(word);
    }
    return words;
}

bool ParseDomainId(const std::string & value, int32_t & domain_id)
{
    try {
        std::size_t parsed_size = 0;
        domain_id = std::stoi(value, &parsed_size);
        return parsed_size == value.size() && domain_id >= 0 && domain_id <= 232;
    } catch (const std::exception &) {
        return false;
    }
}

bool ParseTimeout(const std::string & value, float & timeout_seconds)
{
    try {
        std::size_t parsed_size = 0;
        timeout_seconds = std::stof(value, &parsed_size);
        return parsed_size == value.size() && timeout_seconds > 0.0F;
    } catch (const std::exception &) {
        return false;
    }
}

void PrintAccessInfo(const FileAccessInfo & access)
{
    std::cout << "Access granted: token_received=" << std::boolalpha << !access.token.empty()
              << ", base_url=" << access.base_url << ", expires_at=" << access.expires_at
              << ", ttl_seconds=" << access.ttl_seconds << std::endl;
}

int Revoke(AuthClient & client)
{
    const int32_t ret = client.RevokeAccess();
    if (ret != RPC_OK) {
        std::cerr << "RevokeAccess failed: code=" << ret << ", message=" << RpcErrorDescription(ret) << std::endl;
        return 1;
    }
    std::cout << "Access revoked." << std::endl;
    return 0;
}

}  // namespace

int main(int argc, char const * argv[])
{
    std::map<std::string, std::string> args = {
        {"domain_id",         "0" },
        {"network_interface", ""  },
        {"timeout",           "10"},
    };

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.rfind("--", 0) != 0) {
            std::cerr << "Unexpected positional argument: " << arg << std::endl;
            return 1;
        }

        const std::size_t separator = arg.find('=');
        const std::string key = arg.substr(2, separator == std::string::npos ? std::string::npos : separator - 2);
        const std::string value = separator == std::string::npos ? std::string{} : arg.substr(separator + 1);
        args[key] = value;
    }

    if (argc == 1 || args.count("help") != 0) {
        std::cout << "Usage: auth [--domain_id=<id>] [--network_interface=<iface>] "
                     "[--timeout=<seconds>] <command>\n\n"
                  << "Commands:\n"
                  << "  --request_access[=<type>]        Request raw access metadata\n"
                  << "  --request_file_access            Request parsed file access and "
                     "revoke it\n"
                  << "  --download=\"<remote> <local>\"  Request access, download, then "
                     "revoke it\n\n"
                  << "Examples:\n"
                  << "  auth --request_access=file\n"
                  << "  auth --network_interface=eth0 --request_file_access\n"
                  << "  auth --download=\"/files/maps/map.yaml ./map.yaml\"\n";
        return 0;
    }

    int32_t domain_id = 0;
    float timeout_seconds = 0.0F;
    if (!ParseDomainId(args.at("domain_id"), domain_id)) {
        std::cerr << "Invalid domain_id: " << args.at("domain_id") << std::endl;
        return 1;
    }
    if (!ParseTimeout(args.at("timeout"), timeout_seconds)) {
        std::cerr << "Invalid timeout: " << args.at("timeout") << std::endl;
        return 1;
    }

    const bool request_access = args.count("request_access") != 0;
    const bool request_file_access = args.count("request_file_access") != 0;
    const bool download = args.count("download") != 0;
    if (static_cast<int>(request_access) + static_cast<int>(request_file_access) + static_cast<int>(download) != 1) {
        std::cerr << "Select exactly one command. Run auth --help for usage." << std::endl;
        return 1;
    }

    try {
        ChannelFactory::Instance()->Init(domain_id, args.at("network_interface"));

        AuthClient client;
        client.Init();
        client.SetTimeout(timeout_seconds);

        if (request_access) {
            const std::string type = args.at("request_access").empty() ? "file" : args.at("request_access");
            std::string response;
            const int32_t ret = client.RequestAccess(type, response);
            if (ret != RPC_OK) {
                std::cerr << "RequestAccess failed: code=" << ret << ", message=" << RpcErrorDescription(ret)
                          << std::endl;
                return 1;
            }
            std::cout << "RequestAccess succeeded: type=" << type << ", response_bytes=" << response.size()
                      << std::endl;
            return client.HasValidToken() ? Revoke(client) : 0;
        }

        FileAccessInfo access;
        const int32_t request_ret = client.RequestFileAccess(access);
        if (request_ret != RPC_OK) {
            std::cerr << "RequestFileAccess failed: code=" << request_ret
                      << ", message=" << RpcErrorDescription(request_ret) << std::endl;
            return 1;
        }
        PrintAccessInfo(access);

        int exit_code = 0;
        if (download) {
            const std::vector<std::string> paths = SplitWords(args.at("download"));
            if (paths.size() != 2) {
                std::cerr << "--download requires a remote path and a local path." << std::endl;
                Revoke(client);
                return 1;
            }

            int last_percent = -1;
            const int32_t download_ret =
                client.DownloadFile(paths[0], paths[1], [&last_percent](int64_t downloaded, int64_t total) {
                    if (total <= 0) {
                        return;
                    }
                    const int percent = static_cast<int>(downloaded * 100 / total);
                    if (percent != last_percent) {
                        std::cout << "\rDownloading: " << percent << "%" << std::flush;
                        last_percent = percent;
                    }
                });
            if (last_percent >= 0) {
                std::cout << std::endl;
            }
            if (download_ret != RPC_OK) {
                std::cerr << "DownloadFile failed: code=" << download_ret << std::endl;
                exit_code = 1;
            } else {
                std::cout << "Downloaded to " << paths[1] << std::endl;
            }
        }

        if (Revoke(client) != 0) {
            exit_code = 1;
        }
        return exit_code;
    } catch (const std::exception & error) {
        std::cerr << "Auth example failed: " << error.what() << std::endl;
        return 2;
    }
}

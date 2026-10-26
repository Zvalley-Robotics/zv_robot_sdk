#pragma once

#include <curl/curl.h>

#include <chrono>
#include <cstdio>
#include <string>
#include <zv/common/file_info.hpp>
#include <zv/robot/client/client.hpp>

namespace zv::robot
{

// DDS 鉴权客户端：向 "auth" 服务申请/释放 session，供后续 HTTP 文件下载使用。
class AuthClient : public Client
{
public:
    AuthClient();
    ~AuthClient() override = default;

    void Init();

    // 通用鉴权申请：type 区分用途（"file" / "api" 等），out_data 为服务端返回的原始 JSON
    int32_t RequestAccess(const std::string & type, std::string & out_data);

    // 文件下载专用封装：解析 out_data 填充 FileAccessInfo，并缓存 token/base_url
    int32_t RequestFileAccess(FileAccessInfo & info);

    // DDS：释放当前 session
    int32_t RevokeAccess();

    bool HasValidToken() const;

    // HTTP 下载：需持有有效 token，否则立即返回 -1
    int32_t DownloadFile(const std::string & remote_path, const std::string & local_path,
                         DownloadProgressFn progress = nullptr);

    const std::string & token() const { return token_; }
    const std::string & base_url() const { return base_url_; }

protected:
    std::string token_;
    std::string base_url_;
    std::chrono::system_clock::time_point expires_at_;

private:
    struct WriteCtx_
    {
        FILE * fp{nullptr};
        int64_t * downloaded{nullptr};
        DownloadProgressFn progress;
    };

    static size_t writeCb_(char * ptr, size_t size, size_t nmemb, void * userdata);
    static int xferInfoCb_(void * clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t);
    int32_t downloadFile_(const std::string & url, const std::string & token, const std::string & local_path,
                          DownloadProgressFn progress = nullptr);
};

}  // namespace zv::robot

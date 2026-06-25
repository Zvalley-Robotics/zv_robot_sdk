#pragma once

#include <mutex>

#include <zv/common/dds/dds_factory_model.hpp>
#include <zv/third_party/nlohmann_json/json.hpp>

namespace zv::robot
{

template<typename MSG>
using Channel = zv::common::DdsTopicChannel<MSG>;

template<typename MSG>
using ChannelPtr = zv::common::DdsTopicChannelPtr<MSG>;

class ChannelFactory
{
public:
    static ChannelFactory* Instance()
    {
        static ChannelFactory instance;
        return &instance;
    }

    void Init(int32_t domain_id, const std::string& network_interface = "");
    void Init(const nlohmann::json &config);

    void Release();

    template<typename MSG>
    ChannelPtr<MSG> CreateSendChannel(const std::string& name)
    {
        ChannelPtr<MSG> channel_ptr = dds_factory_ptr_->CreateTopicChannel<MSG>(name);
        dds_factory_ptr_->SetWriter(channel_ptr);
        return channel_ptr;
    }

    template<typename MSG>
    ChannelPtr<MSG> CreateRecvChannel(const std::string& name, std::function<void(const void*)> callback, int32_t queue_len = 0)
    {
        ChannelPtr<MSG> channel_ptr = dds_factory_ptr_->CreateTopicChannel<MSG>(name);
        dds_factory_ptr_->SetReader(channel_ptr, callback, queue_len);
        return channel_ptr;
    }

public:
    ~ChannelFactory();

private:
    ChannelFactory();

private:
    bool initialized_ = false;
    common::DdsFactoryModelPtr dds_factory_ptr_;
    std::mutex mutex_;
};

}

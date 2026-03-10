#pragma once


#include <iostream>

#include <zv/robot/channel/channel_factory.hpp>

namespace zv::robot
{

template<typename MSG>
class ChannelSubscriber
{
public:
    explicit ChannelSubscriber(const std::string& channel_name) :
        channel_name_(channel_name), queue_len_(0){}

    explicit ChannelSubscriber(const std::string& channel_name, const std::function<void(const void*)>& handler, int64_t queue_len = 0) :
        channel_name_(channel_name), queue_len_(queue_len), handler_(handler){}

    void InitChannel(const std::function<void(const void*)>& handler, int64_t queue_len = 0)
    {
        handler_ = handler;
        queue_len_ = queue_len;

        InitChannel();
    }

    void InitChannel()
    {
        if (handler_)
        {
            channel_ptr_ = ChannelFactory::Instance()->CreateRecvChannel<MSG>(channel_name_, handler_, queue_len_);
        }
        else
        {
            std::cerr << "ChannelSubscriber::InitChannel: handler is invalid" << std::endl;
        }
    }

    void CloseChannel()
    {
        channel_ptr_.reset();
    }

    int64_t GetLastDataAvailableTime() const
    {
        if (channel_ptr_)
        {
            return channel_ptr_->GetLastDataAvailableTime();
        }

        return -1;
    }

    const std::string& GetChannelName() const
    {
        return channel_name_;
    }

private:
    std::string channel_name_;
    int64_t queue_len_;
    std::function<void(const void*)> handler_;
    ChannelPtr<MSG> channel_ptr_;
};

template<typename MSG>
using ChannelSubscriberPtr = std::shared_ptr<ChannelSubscriber<MSG>>;


}
#pragma once


#include <string>
#include <memory>

#include <zv/robot/channel/channel_factory.hpp>

namespace zv::robot
{

template<typename MSG>
class ChannelPublisher
{
public:
    explicit ChannelPublisher(const std::string& channel_name) : channel_name_(channel_name){}

    void InitChannel()
    {
        channel_ptr_ = ChannelFactory::Instance()->CreateSendChannel<MSG>(channel_name_);
    }

    void Write(const MSG& msg, int64_t wait_microsec = 0)
    {
        if (channel_ptr_)
        {
            channel_ptr_->Write(msg, wait_microsec);
        }
    }

    void CloseChannel()
    {
        channel_ptr_.reset();
    }

    const std::string& GetChannelName() const
    {
        return channel_name_;
    }

private:
    std::string channel_name_;
    ChannelPtr<MSG> channel_ptr_;
};

template<typename MSG>
using ChannelPublisherPtr = std::shared_ptr<ChannelPublisher<MSG>>;

}
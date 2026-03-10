#pragma once

#include <zv/common/dds/dds_topic_channel.hpp>
#include <zv/third_party/nlohmann_json/json.hpp>

namespace zv::common
{

class DdsFactoryModel
{
public:
    explicit DdsFactoryModel();
    ~DdsFactoryModel();

    void Init(uint32_t domain_id, const std::string &network_interface);
    void Init(const nlohmann::json &config);

    template<typename MSG>
    DdsTopicChannelPtr<MSG> CreateTopicChannel(const std::string& topic)
    {
        DdsTopicChannelPtr<MSG> channel = DdsTopicChannelPtr<MSG>(new DdsTopicChannel<MSG>());
        channel->SetTopic(participant_, topic, topic_qos_);
        return channel;
    }

    template<typename MSG>
    void SetWriter(DdsTopicChannelPtr<MSG>& channel_ptr)
    {
        channel_ptr->SetWriter(publisher_, writer_qos_);
    }

    template<typename MSG>
    void SetReader(DdsTopicChannelPtr<MSG>& channel_ptr, const std::function<void(const void*)>& handler, int32_t queue_len = 0)
    {
        DdsReaderCallback cb(handler);
        channel_ptr->SetReader(subscriber_, reader_qos_, cb, queue_len);
    }

private:
    DdsParticipantPtr participant_;
    DdsPublisherPtr publisher_;
    DdsSubscriberPtr subscriber_;

    DdsParticipantQos participant_qos_;
    DdsTopicQos topic_qos_;
    DdsPublisherQos publisher_qos_;
    DdsSubscriberQos subscriber_qos_;
    DdsWriterQos writer_qos_;
    DdsReaderQos reader_qos_;

    //Logger *mLogger;
};

using DdsFactoryModelPtr = std::shared_ptr<DdsFactoryModel>;

}

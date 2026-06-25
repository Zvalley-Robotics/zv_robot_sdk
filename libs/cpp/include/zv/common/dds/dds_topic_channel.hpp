#pragma once

//#include <iostream>
#include <thread>

#include <zv/common/dds/dds_entity.hpp>
#include <zv/common/dds/dds_qos.hpp>

using namespace org::eclipse::cyclonedds;

namespace zv::common
{

template <typename MSG>
class DdsTopicChannel
{
public:
  explicit DdsTopicChannel(){}

  ~DdsTopicChannel(){}

  void SetTopic(const DdsParticipantPtr& participant, const std::string& name, const DdsTopicQos& qos)
  {
    topic_ = std::make_shared<DdsTopic<MSG>>(getRef(participant), name, qos);
  }

  void SetWriter(const DdsPublisherPtr& publisher, const DdsWriterQos& qos)
  {
    writer_ = std::make_shared<DdsWriter<MSG>>(getRef(publisher), getRef(topic_), qos);
    // MicroSleep(UT_DDS_WAIT_MATCHED_TIME_MICRO_SEC);
  }

  void SetReader(const DdsSubscriberPtr& subscriber, const DdsReaderQos& qos, const DdsReaderCallback& cb, int32_t queue_len)
  {
    reader_ = std::make_shared<DdsReader<MSG>>(getRef(subscriber), getRef(topic_), qos);
    listener_ = std::make_shared<DdsReaderListener<MSG>>();
    listener_->SetCallback(cb);
    reader_->listener(listener_.get(), listener_->GetStatusMask());
  }

  DdsWriterPtr<MSG> GetWriter() const
  {
    return writer_;
  }

  DdsReaderPtr<MSG> GetReader() const
  {
    return reader_;
  }

  void Write(const void* message, int64_t waitMicrosec)
  {
    Write(*(const MSG*)message, waitMicrosec);
  }

  void Write(const MSG& message, int64_t waitMicrosec)
  {
    if (waitMicrosec >= ZV_DDS_WAIT_MATCHED_TIME_MIN)
    {
      int64_t waitTime = (waitMicrosec / 2);
      if (waitTime > ZV_DDS_WAIT_MATCHED_TIME_MAX)
      {
          waitTime = ZV_DDS_WAIT_MATCHED_TIME_MAX;
      }

      while (waitTime > 0 && writer_->publication_matched_status().current_count() == 0)
      {
          std::this_thread::sleep_for(std::chrono::microseconds(ZV_DDS_WAIT_MATCHED_TIME_MIN));
          waitTime -=ZV_DDS_WAIT_MATCHED_TIME_MIN;
      }
    }

    writer_->write(message, waitMicrosec);
  }

  int64_t GetLastDataAvailableTime() const
  {
    if (reader_)
    {
      return reader_->GetLastDataAvailableTime();
    }

    return 0;
  }

private:
  DdsWriterPtr<MSG> writer_;
  DdsReaderPtr<MSG> reader_;
  DdsTopicPtr<MSG> topic_;
  DdsReaderListenerPtr<MSG> listener_;
};

template <typename MSG>
using DdsTopicChannelPtr = std::shared_ptr<DdsTopicChannel<MSG>>;

}
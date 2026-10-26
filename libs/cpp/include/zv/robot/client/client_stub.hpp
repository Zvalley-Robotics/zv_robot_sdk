#pragma once

#include <zv/robot/channel/channel_publisher.hpp>
#include <zv/robot/channel/channel_subscriber.hpp>
#include <zv/robot/client/client_diagnostics.hpp>
#include <zv/robot/future/request_future.hpp>
// #include <zv/common/block_queue.hpp>

namespace zv::robot
{

class ClientStub
{
public:
    explicit ClientStub();
    ~ClientStub();

    void Init(const std::string & name);

    bool Send(const Request_ & req, int64_t waitTimeout);
    RequestFuturePtr SendRequest(const Request_ & req, int64_t waitTimeout);

    TopicMatchInfo GetTopicMatchInfo() const;
    bool WaitForRequestMatched(int64_t timeout) const;
    bool WaitForTopicMatched(int64_t timeout) const;
    void CancelRequest(int64_t requestId);

private:
    void ResponseFunc(const void * message);

private:
    ChannelPublisherPtr<Request_> mRequestPublisherPtr;
    ChannelSubscriberPtr<Response_> mResponseSubscriberPtr;
    RequestFutureQueuePtr mFutureQueuePtr;
    std::string mRequestTopic;
    std::string mResponseTopic;
};

using ClientStubPtr = std::shared_ptr<ClientStub>;

}  // namespace zv::robot

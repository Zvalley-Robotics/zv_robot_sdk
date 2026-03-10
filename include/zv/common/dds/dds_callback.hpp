#pragma once

#include <memory>
#include <functional>

using DdsMessageHandler = std::function<void(const void*)>;

namespace zv::common
{

class DdsReaderCallback
{
public:
  DdsReaderCallback();
  explicit DdsReaderCallback(const DdsMessageHandler& handler) : handler_(handler){};
  DdsReaderCallback(const DdsReaderCallback& cb);
  DdsReaderCallback& operator=(const DdsReaderCallback& cb);

  ~DdsReaderCallback();

  bool HasMessageHandler() const;
  void OnDataAvailable(const void* data);

private:
  DdsMessageHandler handler_;
};

using DdsReaderCallbackPtr = std::shared_ptr<DdsReaderCallback>;

}
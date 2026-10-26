#pragma once

#include <zv/robot/internal/idl/Request_.hpp>
#include <zv/robot/internal/idl/Response_.hpp>

namespace zv::robot
{
using RequestIdentity_ = zv_api::msg::dds_::RequestIdentity_;
using RequestPolicy_ = zv_api::msg::dds_::RequestPolicy_;
using RequestHeader_ = zv_api::msg::dds_::RequestHeader_;
using Request_ = zv_api::msg::dds_::Request_;

using ResponseStatus_ = zv_api::msg::dds_::ResponseStatus_;
using ResponseHeader_ = zv_api::msg::dds_::ResponseHeader_;
using Response_ = zv_api::msg::dds_::Response_;
}  // namespace zv::robot

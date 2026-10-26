#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <zv/robot/internal/internal_request_response.hpp>

namespace zv::robot
{
const int32_t ROBOT_INTERNAL_API_ID_MAX = 100;

const int32_t ROBOT_API_ID_NONE = -1;
const int32_t ROBOT_API_ID_INTERNAL_API_VERSION = 1;

#define IS_INTERNAL_API(apiId) ((apiId) <= ROBOT_INTERNAL_API_ID_MAX)

#define ZV_DECL_ERR(name, code, desc) \
    const int32_t name = code;        \
    const std::string name##_DESC = desc;

// clang-format off
ZV_DECL_ERR(RPC_OK,                            0,      "Success.")
ZV_DECL_ERR(RPC_ERR_UNKNOWN,                   3001,   "Unknown error.")
ZV_DECL_ERR(RPC_ERR_CLIENT_SEND,               3102,   "Send request error.")
ZV_DECL_ERR(RPC_ERR_CLIENT_API_NOT_REG,        3103,   "API is not registered.")
ZV_DECL_ERR(RPC_ERR_CLIENT_API_TIMEOUT,        3104,   "DDS topics are matched, but the API response timed out.")
ZV_DECL_ERR(RPC_ERR_CLIENT_API_NOT_MATCH,      3105,   "Response api not match error.")
ZV_DECL_ERR(RPC_ERR_CLIENT_API_DATA,           3106,   "Response data error.")
ZV_DECL_ERR(RPC_ERR_CLIENT_LEASE_INVALID,      3107,   "Lease is invalid.")
ZV_DECL_ERR(RPC_ERR_CLIENT_REQUEST_TOPIC_NOT_MATCHED,  3108,   "DDS request topic has no matched subscriber.")
ZV_DECL_ERR(RPC_ERR_CLIENT_RESPONSE_TOPIC_NOT_MATCHED, 3109,   "DDS response topic has no matched publisher.")

ZV_DECL_ERR(RPC_ERR_SERVER_SEND,               3201,   "Send response error.")
ZV_DECL_ERR(RPC_ERR_SERVER_INTERNAL,           3202,   "Server internal error.")
ZV_DECL_ERR(RPC_ERR_SERVER_API_NOT_IMPL,       3203,   "Api not implement error.")
ZV_DECL_ERR(RPC_ERR_SERVER_API_PARAMETER,      3204,   "Api parameter error.")
ZV_DECL_ERR(RPC_ERR_SERVER_LEASE_DENIED,       3205,   "Request denied by lease.")
ZV_DECL_ERR(RPC_ERR_SERVER_LEASE_NOT_EXIST,    3206,   "Lease not exist in server cache.")
ZV_DECL_ERR(RPC_ERR_SERVER_LEASE_EXIST,        3207,   "Lease is already exist in server cache.")
// clang-format on

const std::string & RpcErrorDescription(int32_t code);

using RequestPtr = std::shared_ptr<Request_>;
using ResponsePtr = std::shared_ptr<Response_>;

}  // namespace zv::robot

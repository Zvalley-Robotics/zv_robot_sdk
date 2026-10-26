#pragma once

#include <string>
#include <vector>
#include <zv/common/file_info.hpp>

namespace zv::robot
{

int32_t parse_access_response(const std::string & json, FileAccessInfo & out);
int32_t parse_list_response(const std::string & json, std::vector<FileInfo> & out);

}  // namespace zv::robot

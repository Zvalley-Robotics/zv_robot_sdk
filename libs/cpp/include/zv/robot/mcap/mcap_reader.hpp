#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace zv::robot
{

enum class McapReadResult { kMessage, kEndOfFile, kError };

// Owns all data, including schema and payload, independently of the reader.
// Timestamps are nanoseconds. Payload remains encoded (e.g. ROS2 CDR).
struct McapMessage
{
    std::string topic;
    std::string message_encoding;
    bool has_schema{false};
    std::string schema_name;
    std::string schema_encoding;
    std::vector<uint8_t> schema_data;
    uint32_t sequence{0};
    uint64_t log_time{0};
    uint64_t publish_time{0};
    std::vector<uint8_t> data;

    // Business preview JSON: status, robot, business, fields or error.
    // Decodes the listed T3/T4 topics using embedded ros2msg + CDR1.
    // Does not change the raw payload; unsupported/invalid data is reported
    // in the JSON. Large arrays are summarized for display.
    std::string DecodeJson() const;
};

// Local sequential reader; no DDS initialization or robot connection
// required. Supports uncompressed, LZ4 and Zstd files, with or without a
// summary. One instance must not be used concurrently. Separate instances
// are independent.
class McapReader
{
public:
    McapReader();
    ~McapReader();
    McapReader(const McapReader &) = delete;
    McapReader & operator=(const McapReader &) = delete;

    // Closes any previous file. False means failure; see LastError().
    bool Open(const std::string & path);
    // Resets output on EOF/error. Errors remain sticky until
    // Open()/Close().
    McapReadResult ReadNext(McapMessage & output);
    // Idempotent; clears LastError(). Open() can be used again afterwards.
    void Close() noexcept;
    bool IsOpen() const noexcept;
    std::string LastError() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace zv::robot

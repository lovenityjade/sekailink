#pragma once

#include "sekailink_sklmi/api.hpp"

#include <filesystem>
#include <fstream>
#include <cstdint>

namespace sekailink::sklmi {

class JsonlFileEventSink final : public EventSink {
  public:
    static constexpr std::uintmax_t kDefaultMaxBytes = 16U * 1024U * 1024U;
    static constexpr std::size_t kDefaultMaxRecordBytes = 64U * 1024U;

    explicit JsonlFileEventSink(
        std::filesystem::path path,
        std::uintmax_t max_bytes = kDefaultMaxBytes,
        std::size_t max_record_bytes = kDefaultMaxRecordBytes);

    bool good() const;
    const std::filesystem::path& path() const;
    void emit(const Event& event) override;
    void trace(const TraceRecord& record) override;

  private:
    void append_line(const std::string& line);
    bool rotate_if_needed(std::size_t incoming_bytes);
    bool open_output(std::ios::openmode mode);

    std::filesystem::path path_;
    std::filesystem::path rotated_path_;
    std::ofstream output_;
    std::uintmax_t current_bytes_ = 0;
    std::uintmax_t max_bytes_ = kDefaultMaxBytes;
    std::size_t max_record_bytes_ = kDefaultMaxRecordBytes;
};

}  // namespace sekailink::sklmi

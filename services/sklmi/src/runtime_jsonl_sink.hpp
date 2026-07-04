#pragma once

#include "sekailink_sklmi/api.hpp"

#include <filesystem>
#include <fstream>

namespace sekailink::sklmi {

class JsonlFileEventSink final : public EventSink {
  public:
    explicit JsonlFileEventSink(std::filesystem::path path);

    bool good() const;
    const std::filesystem::path& path() const;
    void emit(const Event& event) override;
    void trace(const TraceRecord& record) override;

  private:
    void append_line(const std::string& line);

    std::filesystem::path path_;
    std::ofstream output_;
};

}  // namespace sekailink::sklmi

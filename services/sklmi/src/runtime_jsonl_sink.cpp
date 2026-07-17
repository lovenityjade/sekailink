#include "runtime_jsonl_sink.hpp"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <system_error>

namespace sekailink::sklmi {

namespace {

std::string EscapeJson(std::string_view value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (const char ch : value) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

std::string LogLevelName(LogLevel level) {
    switch (level) {
        case LogLevel::trace: return "trace";
        case LogLevel::info: return "info";
        case LogLevel::warning: return "warning";
        case LogLevel::error: return "error";
    }
    return "info";
}

std::string EventToJson(const Event& event) {
    std::ostringstream out;
    out << "{\"record_type\":\"event\""
        << ",\"event_type\":\"" << EscapeJson(format_event_type(event.type)) << "\""
        << ",\"key\":\"" << EscapeJson(event.key) << "\""
        << ",\"value\":\"" << EscapeJson(event.value) << "\"";
    if (!event.driver_instance_id.empty()) {
        out << ",\"driver_instance_id\":\"" << EscapeJson(event.driver_instance_id) << "\"";
    }
    if (!event.linkedworld_id.empty()) {
        out << ",\"linkedworld_id\":\"" << EscapeJson(event.linkedworld_id) << "\"";
    }
    if (!event.core_profile.empty()) {
        out << ",\"core_profile\":\"" << EscapeJson(event.core_profile) << "\"";
    }
    if (event.canonical_id != 0) {
        out << ",\"canonical_id\":" << event.canonical_id;
    }
    if (event.player_number != 0) {
        out << ",\"player_number\":" << event.player_number;
    }
    if (!event.tab_id.empty()) {
        out << ",\"tab_id\":\"" << EscapeJson(event.tab_id) << "\"";
    }
    if (!event.map_id.empty()) {
        out << ",\"map_id\":\"" << EscapeJson(event.map_id) << "\"";
    }
    if (!event.zone_id.empty()) {
        out << ",\"zone_id\":\"" << EscapeJson(event.zone_id) << "\"";
    }
    out << "}";
    return out.str();
}

std::string TraceToJson(const TraceRecord& record) {
    std::ostringstream out;
    out << "{\"record_type\":\"trace\""
        << ",\"level\":\"" << EscapeJson(LogLevelName(record.level)) << "\""
        << ",\"source\":\"" << EscapeJson(record.source) << "\""
        << ",\"event\":\"" << EscapeJson(record.event) << "\""
        << ",\"detail\":\"" << EscapeJson(record.detail) << "\""
        << ",\"tick_index\":" << record.tick_index
        << ",\"monotonic_ms\":" << record.monotonic_ms
        << "}";
    return out.str();
}

}  // namespace

JsonlFileEventSink::JsonlFileEventSink(std::filesystem::path path,
                                       std::uintmax_t max_bytes,
                                       std::size_t max_record_bytes)
    : path_(std::move(path)),
      rotated_path_(path_.string() + ".1"),
      max_bytes_(std::max<std::uintmax_t>(max_bytes, 1)),
      max_record_bytes_(std::max<std::size_t>(max_record_bytes, 1)) {
    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);

    ec.clear();
    const auto existing_bytes = std::filesystem::file_size(path_, ec);
    if (!ec && existing_bytes > max_bytes_) {
        std::filesystem::remove(path_, ec);
    }
    ec.clear();
    const auto rotated_bytes = std::filesystem::file_size(rotated_path_, ec);
    if (!ec && rotated_bytes > max_bytes_) {
        std::filesystem::remove(rotated_path_, ec);
    }
    open_output(std::ios::app);
}

bool JsonlFileEventSink::good() const {
    return output_.good();
}

const std::filesystem::path& JsonlFileEventSink::path() const {
    return path_;
}

void JsonlFileEventSink::emit(const Event& event) {
    append_line(EventToJson(event));
}

void JsonlFileEventSink::trace(const TraceRecord& record) {
    // Successful per-tick traces can occur more than 100 times per second and
    // do not carry recovery state. Keep warnings/errors and lifecycle records.
    if (record.level == LogLevel::trace) return;
    append_line(TraceToJson(record));
}

void JsonlFileEventSink::append_line(const std::string& line) {
    if (!output_) return;
    std::string bounded_line = line;
    if (bounded_line.size() > max_record_bytes_) {
        bounded_line = "{\"record_type\":\"trace\",\"level\":\"warning\","
                       "\"source\":\"jsonl_sink\",\"event\":\"record_omitted\","
                       "\"detail\":\"record exceeded safe size\",\"original_bytes\":" +
                       std::to_string(line.size()) + "}";
    }
    if (!rotate_if_needed(bounded_line.size() + 1)) return;
    output_ << bounded_line << "\n";
    output_.flush();
    if (output_) current_bytes_ += bounded_line.size() + 1;
}

bool JsonlFileEventSink::rotate_if_needed(std::size_t incoming_bytes) {
    if (current_bytes_ == 0 || current_bytes_ + incoming_bytes <= max_bytes_) return true;

    output_.flush();
    output_.close();
    std::error_code ec;
    std::filesystem::remove(rotated_path_, ec);
    ec.clear();
    std::filesystem::rename(path_, rotated_path_, ec);
    if (ec) {
        ec.clear();
        std::filesystem::remove(path_, ec);
    }
    current_bytes_ = 0;
    return open_output(std::ios::trunc);
}

bool JsonlFileEventSink::open_output(std::ios::openmode mode) {
    output_.open(path_, mode);
    if (!output_) return false;
    std::error_code ec;
    current_bytes_ = std::filesystem::file_size(path_, ec);
    if (ec) current_bytes_ = 0;
    return true;
}

}  // namespace sekailink::sklmi

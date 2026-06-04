#include "runtime_jsonl_sink.hpp"

#include <filesystem>
#include <sstream>

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

JsonlFileEventSink::JsonlFileEventSink(std::filesystem::path path) : path_(std::move(path)) {
    std::filesystem::create_directories(path_.parent_path());
    output_.open(path_, std::ios::app);
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
    append_line(TraceToJson(record));
}

void JsonlFileEventSink::append_line(const std::string& line) {
    if (!output_) return;
    output_ << line << "\n";
    output_.flush();
}

}  // namespace sekailink::sklmi

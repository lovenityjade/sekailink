#include "runtime_debug_log.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace sekaiemu::spike {
namespace {

constexpr std::size_t kMaxEvents = 900;
constexpr std::size_t kSnapshotHashMod = 1000000007;

std::string JsonToString(const nlohmann::json& value) {
  try {
    return value.dump();
  } catch (...) {
    return {};
  }
}

std::string LowerCopy(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return text;
}

std::string JsonScalarToString(const nlohmann::json& value) {
  if (value.is_string()) {
    return value.get<std::string>();
  }
  if (value.is_boolean()) {
    return value.get<bool>() ? "true" : "false";
  }
  if (value.is_number_integer()) {
    return std::to_string(value.get<long long>());
  }
  if (value.is_number_unsigned()) {
    return std::to_string(value.get<unsigned long long>());
  }
  if (value.is_number_float()) {
    return std::to_string(value.get<double>());
  }
  return {};
}

std::string FirstString(const nlohmann::json& value, std::initializer_list<const char*> keys) {
  if (!value.is_object()) {
    return {};
  }
  for (const char* key : keys) {
    const auto found = value.find(key);
    if (found != value.end()) {
      auto text = JsonScalarToString(*found);
      if (!text.empty()) {
        return text;
      }
    }
  }
  return {};
}

const nlohmann::json* ObjectValue(const nlohmann::json& value, std::string_view key) {
  if (!value.is_object()) {
    return nullptr;
  }
  const auto found = value.find(std::string(key));
  return found == value.end() ? nullptr : &*found;
}

std::string FieldLabel(const nlohmann::json& value,
                       std::initializer_list<const char*> name_keys,
                       std::initializer_list<const char*> id_keys,
                       std::string_view id_label) {
  const auto name = FirstString(value, name_keys);
  const auto id = FirstString(value, id_keys);
  std::ostringstream out;
  if (!name.empty()) {
    out << name;
  } else if (!id.empty()) {
    out << id_label << "=" << id;
  } else {
    out << "-";
  }
  if (!id.empty() && id != name) {
    out << " [" << id_label << "=" << id << "]";
  }
  return out.str();
}

std::string JoinFirstStrings(const nlohmann::json& values,
                             std::initializer_list<const char*> name_keys,
                             std::initializer_list<const char*> id_keys,
                             std::string_view id_label,
                             std::size_t max_count) {
  if (!values.is_array() || values.empty()) {
    return {};
  }
  std::ostringstream out;
  const std::size_t count = std::min<std::size_t>(values.size(), max_count);
  for (std::size_t index = 0; index < count; ++index) {
    if (index > 0) {
      out << ", ";
    }
    const auto& entry = values[index];
    out << FieldLabel(entry, name_keys, id_keys, id_label);
  }
  if (values.size() > count) {
    out << ", +" << (values.size() - count) << " more";
  }
  return out.str();
}

std::string SummarizeServerSend(const nlohmann::json& value) {
  const auto* messages = ObjectValue(value, "messages");
  if (messages == nullptr || !messages->is_array() || messages->empty()) {
    return {};
  }
  const auto& first = (*messages)[0];
  const auto cmd = FirstString(first, {"cmd"});
  if (cmd == "LocationChecks") {
    const auto count = FirstString(first, {"count"});
    const auto* locations = ObjectValue(first, "locations");
    const auto* names = ObjectValue(first, "location_names");
    std::ostringstream out;
    out << "Check sent";
    if (!count.empty()) {
      out << " x" << count;
    }
    if (names != nullptr && names->is_array() && !names->empty()) {
      out << ": ";
      const std::size_t shown = std::min<std::size_t>(names->size(), 6);
      for (std::size_t index = 0; index < shown; ++index) {
        if (index > 0) out << ", ";
        out << JsonScalarToString((*names)[index]);
        if (locations != nullptr && locations->is_array() && index < locations->size()) {
          out << " [loc=" << JsonScalarToString((*locations)[index]) << "]";
        }
      }
      if (names->size() > shown) {
        out << ", +" << (names->size() - shown) << " more";
      }
    } else if (locations != nullptr && locations->is_array() && !locations->empty()) {
      out << ": ";
      const std::size_t shown = std::min<std::size_t>(locations->size(), 8);
      for (std::size_t index = 0; index < shown; ++index) {
        if (index > 0) out << ", ";
        out << "loc=" << JsonScalarToString((*locations)[index]);
      }
      if (locations->size() > shown) {
        out << ", +" << (locations->size() - shown) << " more";
      }
    }
    return out.str();
  }
  if (cmd == "Connect") {
    return "AP connect request: slot=" + FirstString(first, {"name"}) +
           " game=" + FirstString(first, {"game"});
  }
  if (cmd == "LocationScouts") {
    return "Location scout request: count=" + FirstString(first, {"count"});
  }
  if (cmd == "Sync") {
    return "AP sync request";
  }
  if (!cmd.empty()) {
    return "AP send: " + cmd;
  }
  return {};
}

std::string SummarizeReceivedItems(const nlohmann::json& value) {
  const auto* items = ObjectValue(value, "items");
  if (items == nullptr || !items->is_array() || items->empty()) {
    return {};
  }
  std::ostringstream out;
  out << "Item received";
  const auto count = FirstString(value, {"count"});
  if (!count.empty()) {
    out << " x" << count;
  }
  out << ": " << JoinFirstStrings(*items, {"item_name", "name"}, {"item"}, "item", 5);
  const auto& first = (*items)[0];
  const auto location = FieldLabel(first, {"location_name"}, {"location"}, "loc");
  const auto player = FirstString(first, {"player", "slot", "slot_id"});
  if (location != "-") {
    out << " from " << location;
  }
  if (!player.empty()) {
    out << " player=" << player;
  }
  return out.str();
}

std::string SummarizeMemoryEvent(const nlohmann::json& value) {
  const auto event = FirstString(value, {"event", "event_type", "cmd", "type"});
  if (event.find("retroarch_bridge_read") == std::string::npos &&
      event.find("retroarch_bridge_write") == std::string::npos) {
    return {};
  }
  std::ostringstream out;
  out << (event.find("write") != std::string::npos ? "Memory write" : "Memory read");
  const auto address = FirstString(value, {"address"});
  const auto size = FirstString(value, {"size"});
  const auto count = FirstString(value, {"count"});
  const auto preview = FirstString(value, {"preview", "value"});
  if (!address.empty()) out << " address=" << address;
  if (!size.empty()) out << " size=" << size;
  if (!preview.empty()) out << " bytes=" << preview;
  if (!count.empty()) out << " #" << count;
  return out.str();
}

std::string SummarizeTrackerCommand(const nlohmann::json& value) {
  const auto cmd = FirstString(value, {"cmd"});
  if (cmd.empty()) {
    return {};
  }
  if (cmd == "tracker.click_pin") {
    std::ostringstream out;
    out << "Tracker command sent: click pin";
    const auto location_id = FirstString(value, {"location_id", "location", "id"});
    const auto button = FirstString(value, {"button"});
    if (!location_id.empty()) out << " loc=" << location_id;
    if (!button.empty()) out << " button=" << button;
    return out.str();
  }
  if (cmd == "tracker.click_item") {
    std::ostringstream out;
    out << "Tracker command sent: click item";
    const auto item_id = FirstString(value, {"item_id", "item", "id"});
    const auto code = FirstString(value, {"code", "key"});
    const auto button = FirstString(value, {"button"});
    if (!code.empty()) out << " code=" << code;
    if (!item_id.empty()) out << " item=" << item_id;
    if (!button.empty()) out << " button=" << button;
    return out.str();
  }
  if (cmd.rfind("tracker.", 0) == 0) {
    return "Tracker command sent: " + cmd;
  }
  if (cmd == "chat.say") {
    const auto text = FirstString(value, {"text", "message"});
    return "Tracker/AP command sent: chat.say" + (text.empty() ? std::string() : ": " + text);
  }
  return {};
}

std::string SummarizeTrackerSnapshot(const nlohmann::json& snapshot) {
  if (!snapshot.is_object()) {
    return {};
  }
  const auto checked = ObjectValue(snapshot, "checked_locations");
  const auto missing = ObjectValue(snapshot, "missing_locations");
  const auto received = ObjectValue(snapshot, "received_items");
  const bool looks_like_tracker_snapshot =
      snapshot.contains("schema") || snapshot.contains("revision") || checked != nullptr ||
      missing != nullptr || received != nullptr || snapshot.contains("active_map_hint") ||
      snapshot.contains("active_tab_hint");
  if (!looks_like_tracker_snapshot) {
    return {};
  }
  const auto checked_count = checked != nullptr && checked->is_array() ? checked->size() : 0;
  const auto missing_count = missing != nullptr && missing->is_array() ? missing->size() : 0;
  const auto received_count = received != nullptr && received->is_array() ? received->size() : 0;
  const auto revision = FirstString(snapshot, {"revision"});
  const auto map = FirstString(snapshot, {"active_map_hint", "active_map", "map"});
  const auto tab = FirstString(snapshot, {"active_tab_hint", "active_tab", "tab"});
  const auto state = FirstString(snapshot, {"state", "status"});

  std::ostringstream out;
  out << "Tracker response: snapshot updated";
  if (!revision.empty()) out << " rev=" << revision;
  out << " checks=" << checked_count << "/" << (checked_count + missing_count);
  out << " items=" << received_count;
  if (!map.empty()) out << " map=" << map;
  if (!tab.empty()) out << " tab=" << tab;
  if (!state.empty()) out << " state=" << state;
  return out.str();
}

RuntimeDebugSeverity SeverityFromText(std::string_view level, std::string_view raw) {
  const auto text = LowerCopy(std::string(level.empty() ? raw : level));
  if (text.find("error") != std::string::npos || text.find("failed") != std::string::npos ||
      text.find("exception") != std::string::npos) {
    return RuntimeDebugSeverity::Error;
  }
  if (text.find("warn") != std::string::npos || text.find("retry") != std::string::npos) {
    return RuntimeDebugSeverity::Warning;
  }
  if (text.find("trace") != std::string::npos || text.find("debug") != std::string::npos) {
    return RuntimeDebugSeverity::Trace;
  }
  return RuntimeDebugSeverity::Info;
}

RuntimeDebugCategory CategoryFromText(std::string_view event, std::string_view raw) {
  const auto text = LowerCopy(std::string(event) + " " + std::string(raw));
  if (text.find("location") != std::string::npos || text.find("check") != std::string::npos) {
    return RuntimeDebugCategory::Check;
  }
  if (text.find("item") != std::string::npos || text.find("received") != std::string::npos) {
    return RuntimeDebugCategory::Item;
  }
  if (text.find("connect") != std::string::npos || text.find("disconnect") != std::string::npos ||
      text.find("roominfo") != std::string::npos || text.find("connection") != std::string::npos) {
    return RuntimeDebugCategory::Connection;
  }
  if (text.find("memory") != std::string::npos || text.find("read") != std::string::npos ||
      text.find("write") != std::string::npos || text.find("socket") != std::string::npos) {
    return RuntimeDebugCategory::Memory;
  }
  if (text.find("save") != std::string::npos || text.find("persist") != std::string::npos ||
      text.find("battery") != std::string::npos) {
    return RuntimeDebugCategory::Persistence;
  }
  if (text.find("tracker") != std::string::npos || text.find("tab") != std::string::npos ||
      text.find("map") != std::string::npos) {
    return RuntimeDebugCategory::Tracker;
  }
  if (text.find("command") != std::string::npos || text.find("admin") != std::string::npos ||
      text.find("give") != std::string::npos) {
    return RuntimeDebugCategory::Command;
  }
  if (text.find("error") != std::string::npos || text.find("failed") != std::string::npos ||
      text.find("exception") != std::string::npos) {
    return RuntimeDebugCategory::Error;
  }
  return RuntimeDebugCategory::Other;
}

std::string SummaryFromJson(const nlohmann::json& value, std::string_view source) {
  const auto record_type = FirstString(value, {"record_type", "cmd", "event", "event_type", "type"});
  const auto event = FirstString(value, {"event_type", "event", "cmd", "type"});
  const auto detail = FirstString(value, {"summary", "detail", "message", "text", "value", "error"});
  const auto key = FirstString(value, {"key", "location_name", "item_name", "name"});
  const auto canonical_id = FirstString(value, {"canonical_id", "location", "item", "id"});

  if (event == "server_send") {
    if (auto summary = SummarizeServerSend(value); !summary.empty()) {
      return summary;
    }
  }
  if (event == "received_items" || record_type == "ReceivedItems") {
    if (auto summary = SummarizeReceivedItems(value); !summary.empty()) {
      return summary;
    }
  }
  if (auto summary = SummarizeMemoryEvent(value); !summary.empty()) {
    return summary;
  }
  if (auto summary = SummarizeTrackerCommand(value); !summary.empty()) {
    return summary;
  }

  std::ostringstream out;
  if (!event.empty()) {
    out << event;
  } else if (!record_type.empty()) {
    out << record_type;
  } else {
    out << source;
  }
  if (!key.empty()) {
    out << ": " << key;
  } else if (!detail.empty()) {
    out << ": " << detail;
  }
  if (!canonical_id.empty() && out.str().find(canonical_id) == std::string::npos) {
    out << " (" << canonical_id << ")";
  }
  return out.str();
}

bool EventMatchesFilter(const RuntimeDebugEvent& event, RuntimeDebugFilter filter) {
  switch (filter) {
    case RuntimeDebugFilter::All:
      return true;
    case RuntimeDebugFilter::Errors:
      return event.severity == RuntimeDebugSeverity::Error || event.category == RuntimeDebugCategory::Error;
    case RuntimeDebugFilter::Connections:
      return event.category == RuntimeDebugCategory::Connection;
    case RuntimeDebugFilter::Checks:
      return event.category == RuntimeDebugCategory::Check;
    case RuntimeDebugFilter::Items:
      return event.category == RuntimeDebugCategory::Item;
    case RuntimeDebugFilter::Memory:
      return event.category == RuntimeDebugCategory::Memory || event.category == RuntimeDebugCategory::Persistence;
    case RuntimeDebugFilter::Tracker:
      return event.category == RuntimeDebugCategory::Tracker;
    case RuntimeDebugFilter::Commands:
      return event.category == RuntimeDebugCategory::Command;
  }
  return true;
}

std::uint64_t SnapshotMutationHint(const nlohmann::json& snapshot) {
  if (snapshot.is_discarded() || snapshot.is_null()) {
    return 0;
  }
  const auto text = JsonToString(snapshot);
  std::uint64_t hash = 1469598103934665603ull;
  for (unsigned char ch : text) {
    hash ^= ch;
    hash *= 1099511628211ull;
  }
  return hash % kSnapshotHashMod;
}

}  // namespace

const char* RuntimeDebugCategoryName(RuntimeDebugCategory category) {
  switch (category) {
    case RuntimeDebugCategory::Connection: return "Connection";
    case RuntimeDebugCategory::Check: return "Check";
    case RuntimeDebugCategory::Item: return "Item";
    case RuntimeDebugCategory::Memory: return "Memory";
    case RuntimeDebugCategory::Persistence: return "Persistence";
    case RuntimeDebugCategory::Tracker: return "Tracker";
    case RuntimeDebugCategory::Command: return "Command";
    case RuntimeDebugCategory::Error: return "Error";
    case RuntimeDebugCategory::Other: return "Other";
  }
  return "Other";
}

const char* RuntimeDebugSeverityName(RuntimeDebugSeverity severity) {
  switch (severity) {
    case RuntimeDebugSeverity::Trace: return "Trace";
    case RuntimeDebugSeverity::Info: return "Info";
    case RuntimeDebugSeverity::Warning: return "Warning";
    case RuntimeDebugSeverity::Error: return "Error";
  }
  return "Info";
}

std::string RedactRuntimeDebugText(std::string text) {
  auto lowered = LowerCopy(text);
  constexpr std::string_view admin_login = "!admin login ";
  std::size_t login_pos = 0;
  while ((login_pos = lowered.find(admin_login, login_pos)) != std::string::npos) {
    const std::size_t value_start = login_pos + admin_login.size();
    std::size_t value_end = value_start;
    while (value_end < text.size() && text[value_end] != '\n' && text[value_end] != '\r' &&
           text[value_end] != '"' && text[value_end] != '\'' && text[value_end] != ',' &&
           text[value_end] != '}') {
      ++value_end;
    }
    if (value_end > value_start) {
      text.replace(value_start, value_end - value_start, "***redacted***");
      lowered = LowerCopy(text);
      login_pos = value_start + 14;
    } else {
      login_pos += admin_login.size();
    }
  }
  const std::vector<std::string> keys = {
      "password", "passwd", "token", "authorization", "auth", "admin_secret", "room_password"};
  lowered = LowerCopy(text);
  for (const auto& key : keys) {
    std::size_t pos = 0;
    while ((pos = lowered.find(key, pos)) != std::string::npos) {
      std::size_t value_start = lowered.find_first_of(":=", pos + key.size());
      if (value_start == std::string::npos) {
        pos += key.size();
        continue;
      }
      ++value_start;
      while (value_start < text.size() && (text[value_start] == '"' || text[value_start] == '\'' ||
                                           text[value_start] == ' ')) {
        ++value_start;
      }
      std::size_t value_end = value_start;
      while (value_end < text.size() && text[value_end] != ',' && text[value_end] != '}' &&
             text[value_end] != '\n' && text[value_end] != '\r' && text[value_end] != '"' &&
             text[value_end] != '\'') {
        ++value_end;
      }
      if (value_end > value_start) {
        text.replace(value_start, value_end - value_start, "***redacted***");
        lowered = LowerCopy(text);
        pos = value_start + 14;
      } else {
        pos += key.size();
      }
    }
  }
  return text;
}

void RuntimeDebugLog::Refresh(const BridgeRuntimeStatus& status, const nlohmann::json& snapshot) {
  if (!status.sekaiemu_log_path.empty()) {
    TailSekaiemuRuntimeLog(status.sekaiemu_log_path);
  }
  if (!status.trace_log_path.empty()) {
    TailJsonlFile(status.trace_log_path, "SKLMI", RuntimeDebugCategory::Other);
  }
  if (!status.tracker_command_log_path.empty()) {
    TailJsonlFile(status.tracker_command_log_path, "Tracker command log", RuntimeDebugCategory::Tracker);
  }
  if (!status.companion_log_path.empty()) {
    TailTextFile(status.companion_log_path, "SKLMI companion");
  }
  AppendSnapshotEvents(snapshot);
  Trim();
}

void RuntimeDebugLog::Clear() {
  events_.clear();
}

void RuntimeDebugLog::AddLocalEvent(RuntimeDebugSeverity severity,
                                    RuntimeDebugCategory category,
                                    std::string source,
                                    std::string summary,
                                    std::string raw) {
  AppendEvent(severity, category, std::move(source), std::move(summary), std::move(raw));
  Trim();
}

std::vector<const RuntimeDebugEvent*> RuntimeDebugLog::FilteredEvents(RuntimeDebugFilter filter,
                                                                      std::size_t max_count) const {
  std::vector<const RuntimeDebugEvent*> out;
  for (auto it = events_.rbegin(); it != events_.rend(); ++it) {
    if (!EventMatchesFilter(*it, filter)) {
      continue;
    }
    out.push_back(&*it);
    if (out.size() >= max_count) {
      break;
    }
  }
  std::reverse(out.begin(), out.end());
  return out;
}

std::string RuntimeDebugLog::BuildRedactedReport(const BridgeRuntimeStatus& status,
                                                 const nlohmann::json& snapshot,
                                                 RuntimeDebugFilter filter) const {
  std::ostringstream out;
  out << "SekaiLink Runtime Debug Report\n";
  out << "game=" << status.game_name << "\n";
  out << "slot=" << status.ap_slot_name << "\n";
  out << "endpoint=" << status.ap_host << ":" << status.ap_port << status.ap_path << "\n";
  out << "memory_socket=" << status.runtime_memory_socket_path << "\n";
  out << "sekaiemu_log=" << status.sekaiemu_log_path << "\n";
  out << "trace_log=" << status.trace_log_path << "\n";
  out << "tracker_snapshot=" << status.tracker_snapshot_path << "\n";
  out << "tracker_command_log=" << status.tracker_command_log_path << "\n";
  out << "snapshot=" << RedactRuntimeDebugText(JsonToString(snapshot)) << "\n";
  out << "\nEvents:\n";
  for (const auto* event : FilteredEvents(filter, 240)) {
    out << "#" << event->sequence << " [" << RuntimeDebugSeverityName(event->severity) << "] "
        << RuntimeDebugCategoryName(event->category) << " " << event->source << " - "
        << event->summary << "\n";
    if (!event->raw.empty()) {
      out << "  raw=" << event->raw << "\n";
    }
  }
  return out.str();
}

void RuntimeDebugLog::TailJsonlFile(const std::filesystem::path& path,
                                    std::string_view source,
                                    RuntimeDebugCategory fallback_category) {
  std::error_code ec;
  if (path.empty() || !std::filesystem::exists(path, ec)) {
    return;
  }
  const auto key = path.string();
  auto& state = tail_states_[key];
  const auto file_size = std::filesystem::file_size(path, ec);
  if (ec) {
    return;
  }
  if (state.offset > file_size) {
    state.offset = 0;
  }
  std::ifstream input(path);
  if (!input) {
    return;
  }
  input.seekg(static_cast<std::streamoff>(state.offset));
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty()) {
      continue;
    }
    auto raw = RedactRuntimeDebugText(line);
    try {
      const auto parsed = nlohmann::json::parse(line);
      const auto event_name = FirstString(parsed, {"event_type", "event", "cmd", "type"});
      const auto level = FirstString(parsed, {"level", "severity"});
      const auto severity = SeverityFromText(level, line);
      auto category = CategoryFromText(event_name, line);
      if (category == RuntimeDebugCategory::Other) {
        category = fallback_category;
      }
      AppendEvent(severity, category, std::string(source), SummaryFromJson(parsed, source), raw);
    } catch (...) {
      AppendEvent(SeverityFromText({}, line),
                  CategoryFromText({}, line),
                  std::string(source),
                  line,
                  raw);
    }
  }
  const auto pos = input.tellg();
  state.offset = pos >= 0 ? static_cast<std::uintmax_t>(pos) : file_size;
}

void RuntimeDebugLog::TailTextFile(const std::filesystem::path& path, std::string_view source) {
  std::error_code ec;
  if (path.empty() || !std::filesystem::exists(path, ec)) {
    return;
  }
  const auto key = path.string();
  auto& state = tail_states_[key];
  const auto file_size = std::filesystem::file_size(path, ec);
  if (ec) {
    return;
  }
  if (state.offset > file_size) {
    state.offset = 0;
  }
  std::ifstream input(path);
  if (!input) {
    return;
  }
  input.seekg(static_cast<std::streamoff>(state.offset));
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty()) {
      continue;
    }
    auto raw = RedactRuntimeDebugText(line);
    AppendEvent(SeverityFromText({}, line),
                CategoryFromText({}, line),
                std::string(source),
                raw,
                raw);
  }
  const auto pos = input.tellg();
  state.offset = pos >= 0 ? static_cast<std::uintmax_t>(pos) : file_size;
}

void RuntimeDebugLog::TailSekaiemuRuntimeLog(const std::filesystem::path& path) {
  std::error_code ec;
  if (path.empty() || !std::filesystem::exists(path, ec)) {
    return;
  }
  const auto key = "sekaiemu-runtime:" + path.string();
  auto& state = tail_states_[key];
  const auto file_size = std::filesystem::file_size(path, ec);
  if (ec) {
    return;
  }
  if (state.offset > file_size) {
    state.offset = 0;
  }
  std::ifstream input(path);
  if (!input) {
    return;
  }
  input.seekg(static_cast<std::streamoff>(state.offset));
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty()) {
      continue;
    }
    const auto lowered = LowerCopy(line);
    const bool important =
        line.find("[sekaiemu-memory]") != std::string::npos ||
        lowered.find("sklmi") != std::string::npos ||
        lowered.find("runtime memory") != std::string::npos ||
        lowered.find("memory socket") != std::string::npos ||
        lowered.find("connected") != std::string::npos ||
        lowered.find("disconnect") != std::string::npos ||
        lowered.find("error") != std::string::npos ||
        lowered.find("failed") != std::string::npos;
    if (!important) {
      continue;
    }
    auto raw = RedactRuntimeDebugText(line);
    AppendEvent(SeverityFromText({}, line),
                CategoryFromText({}, line),
                "Sekaiemu runtime",
                raw,
                raw);
  }
  const auto pos = input.tellg();
  state.offset = pos >= 0 ? static_cast<std::uintmax_t>(pos) : file_size;
}

void RuntimeDebugLog::AppendEvent(RuntimeDebugSeverity severity,
                                  RuntimeDebugCategory category,
                                  std::string source,
                                  std::string summary,
                                  std::string raw) {
  RuntimeDebugEvent event;
  event.sequence = next_sequence_++;
  event.severity = severity;
  event.category = category;
  event.source = std::move(source);
  event.summary = RedactRuntimeDebugText(std::move(summary));
  event.raw = RedactRuntimeDebugText(std::move(raw));
  events_.push_back(std::move(event));
}

void RuntimeDebugLog::AppendSnapshotEvents(const nlohmann::json& snapshot) {
  const auto hint = SnapshotMutationHint(snapshot);
  if (hint == 0 || hint == last_snapshot_mutation_hint_) {
    return;
  }
  last_snapshot_mutation_hint_ = hint;
  if (auto summary = SummarizeTrackerSnapshot(snapshot); !summary.empty()) {
    AppendEvent(RuntimeDebugSeverity::Info,
                RuntimeDebugCategory::Tracker,
                "Tracker snapshot",
                std::move(summary),
                JsonToString(snapshot));
  }
  const auto messages = snapshot.is_object() ? snapshot.find("chat_messages") : snapshot.end();
  if (messages != snapshot.end() && messages->is_array()) {
    const std::size_t start = messages->size() > 4 ? messages->size() - 4 : 0;
    for (std::size_t index = start; index < messages->size(); ++index) {
      const auto& message = (*messages)[index];
      if (!message.is_object()) {
        continue;
      }
      auto author = FirstString(message, {"author", "player", "slot"});
      const auto text = FirstString(message, {"text", "message"});
      if (author.empty()) {
        author = "ROOM";
      }
      if (!text.empty()) {
        AppendEvent(RuntimeDebugSeverity::Info,
                    CategoryFromText({}, text),
                    "Room snapshot",
                    author + ": " + text,
                    JsonToString(message));
      }
    }
  }
  const auto received = snapshot.is_object() ? snapshot.find("received_items") : snapshot.end();
  if (received != snapshot.end() && received->is_array() && !received->empty()) {
    const auto& item = received->back();
    const auto item_label = FieldLabel(item, {"item_name", "name"}, {"item"}, "item");
    const auto location_label = FieldLabel(item, {"location_name"}, {"location"}, "loc");
    AppendEvent(RuntimeDebugSeverity::Info,
                RuntimeDebugCategory::Item,
                "Room snapshot",
                "Latest received item: " + item_label +
                    (location_label == "-" ? "" : " from " + location_label),
                JsonToString(item));
  }
}

void RuntimeDebugLog::Trim() {
  if (events_.size() <= kMaxEvents) {
    return;
  }
  events_.erase(events_.begin(), events_.begin() + static_cast<std::ptrdiff_t>(events_.size() - kMaxEvents));
}

}  // namespace sekaiemu::spike

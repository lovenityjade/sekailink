#pragma once

#include "bridge_runtime_status.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sekaiemu::spike {

enum class RuntimeDebugCategory {
  Connection,
  Check,
  Item,
  Memory,
  Persistence,
  Tracker,
  Command,
  Error,
  Other,
};

enum class RuntimeDebugSeverity {
  Trace,
  Info,
  Warning,
  Error,
};

struct RuntimeDebugEvent {
  std::uint64_t sequence = 0;
  RuntimeDebugSeverity severity = RuntimeDebugSeverity::Info;
  RuntimeDebugCategory category = RuntimeDebugCategory::Other;
  std::string source;
  std::string summary;
  std::string raw;
};

enum class RuntimeDebugFilter {
  All,
  Errors,
  Connections,
  Checks,
  Items,
  Memory,
  Tracker,
  Commands,
};

class RuntimeDebugLog {
 public:
  void Refresh(const BridgeRuntimeStatus& status, const nlohmann::json& snapshot);
  void Clear();
  void AddLocalEvent(RuntimeDebugSeverity severity,
                     RuntimeDebugCategory category,
                     std::string source,
                     std::string summary,
                     std::string raw = {});

  const std::vector<RuntimeDebugEvent>& Events() const { return events_; }
  std::vector<const RuntimeDebugEvent*> FilteredEvents(RuntimeDebugFilter filter,
                                                       std::size_t max_count) const;

  std::string BuildRedactedReport(const BridgeRuntimeStatus& status,
                                  const nlohmann::json& snapshot,
                                  RuntimeDebugFilter filter) const;

 private:
  struct TailState {
    std::uintmax_t offset = 0;
  };

  void TailJsonlFile(const std::filesystem::path& path,
                     std::string_view source,
                     RuntimeDebugCategory fallback_category);
  void TailTextFile(const std::filesystem::path& path, std::string_view source);
  void TailSekaiemuRuntimeLog(const std::filesystem::path& path);
  void AppendEvent(RuntimeDebugSeverity severity,
                   RuntimeDebugCategory category,
                   std::string source,
                   std::string summary,
                   std::string raw);
  void AppendSnapshotEvents(const nlohmann::json& snapshot);
  void Trim();

  std::unordered_map<std::string, TailState> tail_states_;
  std::vector<RuntimeDebugEvent> events_;
  std::uint64_t next_sequence_ = 1;
  std::uint64_t last_snapshot_mutation_hint_ = 0;
};

const char* RuntimeDebugCategoryName(RuntimeDebugCategory category);
const char* RuntimeDebugSeverityName(RuntimeDebugSeverity severity);
std::string RedactRuntimeDebugText(std::string text);

}  // namespace sekaiemu::spike

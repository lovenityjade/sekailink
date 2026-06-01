#pragma once

#include "tracker_runtime.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace sekaiemu::spike {

struct RecentRenderRow {
  std::string event_type;
  std::string label;
  std::string timestamp;
  std::string canonical_id;
  std::string detail;
};

struct BundleItemRenderMetadata {
  std::string id;
  std::string code;
  std::string pack_visual_code;
  std::string label;
  std::string abbreviation;
  std::string icon;
  std::int64_t stage = 0;
  std::int64_t count = 0;
  bool acquired = false;
  bool has_explicit_state = false;
};

struct BundlePinSegmentRenderMetadata {
  std::string section_id;
  std::string label;
  std::string color;
  std::int64_t checked_count = 0;
  std::int64_t total_count = 0;
  bool checked = false;
  bool mixed = false;
};

struct BundlePinRenderMetadata {
  std::string id;
  std::string location_id;
  std::string map_id;
  std::string pack_map;
  std::string map_asset;
  std::string label;
  std::string color;
  double x = 0.0;
  double y = 0.0;
  bool has_position = false;
  bool checked = false;
  bool has_explicit_checked = false;
  std::vector<BundlePinSegmentRenderMetadata> segments;
};

const nlohmann::json* JsonValueAtPath(const nlohmann::json& root, std::string_view path);
std::string TruncateText(std::string_view text, std::size_t max_length);
std::string LabelOrId(const nlohmann::json& value);
const TrackerMapDefinition* ResolveActiveMap(const TrackerRuntime& runtime,
                                             const TrackerResolvedViewState& resolved);
std::string ResolveActiveTabLabel(const TrackerResolvedViewState& resolved);
std::string PanelSurface(const nlohmann::json& panel);
int PanelPriority(const nlohmann::json& panel);
std::string JsonScalarToText(const nlohmann::json& value);
std::string JsonStringAtAnyKey(const nlohmann::json& root,
                               std::initializer_list<const char*> keys);
std::optional<double> JsonNumberAtAnyKey(const nlohmann::json& root,
                                         std::initializer_list<const char*> keys);
std::string MetadataStringAt(const nlohmann::json& root, std::string_view path);
std::unordered_set<std::string> ReceivedItemIds(const TrackerRuntime& runtime);
std::unordered_set<std::string> CheckedLocationIds(const TrackerRuntime& runtime);
std::vector<BundleItemRenderMetadata> BuildBundleItems(const TrackerRuntime& runtime);
std::vector<BundlePinRenderMetadata> BuildBundlePins(const TrackerRuntime& runtime,
                                                     const TrackerResolvedViewState& resolved);
std::string SnapshotStringAt(const TrackerRuntime& runtime, std::string_view path);
std::size_t SnapshotArraySize(const nlohmann::json& snapshot,
                              std::initializer_list<const char*> keys);
std::vector<RecentRenderRow> BuildRecentRows(const TrackerRuntime& runtime,
                                             const TrackerResolvedViewState& resolved);
std::size_t CheckedCount(const TrackerRuntime& runtime);
std::size_t MissingCount(const TrackerRuntime& runtime);
std::size_t ReceivedCount(const TrackerRuntime& runtime);
std::string FormatPercent(std::size_t value, std::size_t maximum);
std::string BuildSessionHeadline(const TrackerRuntime& runtime,
                                 const TrackerResolvedViewState& resolved);
std::vector<std::string> BuildMetadataChips(const TrackerRuntime& runtime,
                                            const TrackerResolvedViewState& resolved);
std::string LastReceivedLabel(const TrackerRuntime& runtime,
                              const TrackerResolvedViewState& resolved);
std::string LastReceivedFrom(const TrackerRuntime& runtime);
std::string LastCheckedLabel(const TrackerRuntime& runtime,
                             const TrackerResolvedViewState& resolved);

}  // namespace sekaiemu::spike

#pragma once

#include "tracker_overlay_render_state.hpp"
#include "tracker_overlay_renderer.hpp"
#include "tracker_runtime.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

namespace sekaiemu::spike::tracker_pack_layout_detail {

struct Rect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

struct Size {
  int width = 0;
  int height = 0;
};

struct Margins {
  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
};

struct PackVisualDefinition {
  std::string primary_code;
  std::string type;
  std::string label;
  std::string image;
  std::string disabled_image;
  std::string image_mods;
  std::string disabled_image_mods;
  std::string base_item;
  std::vector<std::string> stages;
  std::vector<std::string> disabled_stages;
  std::vector<std::string> stage_mods;
  std::vector<std::string> disabled_stage_mods;
  std::unordered_map<std::string, int> stage_index_by_alias;
  std::string composite_none_image;
  std::string composite_left_image;
  std::string composite_right_image;
  std::string composite_both_image;
  std::string composite_none_mods;
  std::string composite_left_mods;
  std::string composite_right_mods;
  std::string composite_both_mods;
  std::string item_left;
  std::string item_right;
  int initial_quantity = 0;
  int min_quantity = 0;
  int max_quantity = 0;
  int increment = 1;
  int initial_stage = 0;
  bool loop_stages = false;
  bool static_only = false;
  bool allow_disabled = true;
  bool initial_active_state = false;
};

struct PackVisualState {
  bool acquired = false;
  int stage = 0;
  int count = 0;
  std::string label;
};

struct VisualAssetSelection {
  std::string image;
  std::string mods;
};

struct PackLayoutDocument {
  std::unordered_map<std::string, nlohmann::json> layouts;
  std::unordered_map<std::string, PackVisualDefinition> visuals;
  std::unordered_map<std::string, int> visual_alias_priorities;
  std::unordered_map<std::string, std::vector<std::int64_t>> item_ids_by_code;
  std::unordered_map<std::string, std::string> preferred_layout_by_root;
};

struct PackStateContext {
  const TrackerRuntime& runtime;
  const TrackerResolvedViewState& resolved;
  const TrackerOverlayAssetResolver* asset_resolver = nullptr;
  const TrackerBundle* bundle = nullptr;
  const PackLayoutDocument* document = nullptr;
  std::unordered_map<std::string, PackVisualDefinition> snapshot_visuals;
  std::unordered_map<std::string, int> snapshot_visual_alias_priorities;
  std::unordered_map<std::string, BundleItemRenderMetadata> snapshot_items_by_id;
  std::unordered_map<std::string, std::vector<std::int64_t>> slot_item_ids;
  std::unordered_map<std::int64_t, std::string> slot_id_by_item_id;
  std::unordered_map<std::string, std::string> poptracker_code_by_slot_id;
  std::unordered_map<std::string, std::string> bundle_map_by_pack_map;
  std::unordered_set<std::string> checked_location_ids;
  std::unordered_set<std::string> received_item_ids;
};

enum class RecentPinsStyle {
  Wrap,
  Stack,
};

}  // namespace sekaiemu::spike::tracker_pack_layout_detail

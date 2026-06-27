#include "tracker_pack_layout_interaction.hpp"

#include "tracker_overlay_render_state.hpp"
#include "tracker_overlay_style.hpp"
#include "tracker_pack_layout_document.hpp"
#include "tracker_pack_layout_engine.hpp"
#include "tracker_pack_layout_json.hpp"
#include "tracker_pack_layout_model.hpp"
#include "tracker_pack_layout_widgets.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sekaiemu::spike {
namespace {

using namespace tracker_pack_layout_detail;

std::optional<nlohmann::json> BuildPlayerFocusedRoot(const PackLayoutDocument& document,
                                                      int width,
                                                      int height) {
  const bool wide = width >= height;
  const auto map_key = wide ? std::string{"tabbed_maps_horizontal"} : std::string{"tabbed_maps_vertical"};
  const auto item_key = wide ? std::string{"shared_item_grid_vertical"} : std::string{"shared_item_grid_horizontal"};
  if (!document.layouts.contains(map_key) || !document.layouts.contains(item_key)) {
    return std::nullopt;
  }
  nlohmann::json item_group = {
      {"type", "group"},
      {"header", "Items"},
      {"content", {{"type", "layout"}, {"key", item_key}, {"h_alignment", "center"}, {"v_alignment", "center"}}},
  };
  if (wide) {
    const int item_width = std::max(180, std::min(340, width * 34 / 100));
    item_group["dock"] = "right";
    item_group["min_width"] = std::min(160, item_width);
    item_group["max_width"] = item_width;
  } else {
    const int item_height = std::max(72, std::min(210, height * 32 / 100));
    item_group["dock"] = "bottom";
    item_group["min_height"] = std::min(72, item_height);
    item_group["max_height"] = item_height;
  }
  return nlohmann::json{{"type", "dock"},
                        {"content", nlohmann::json::array({item_group, {{"type", "layout"}, {"key", map_key}}})}};
}

bool PinChecked(const BundlePinRenderMetadata& pin, const std::unordered_set<std::string>& checked_ids) {
  const auto state_id = pin.location_id.empty() ? pin.id : pin.location_id;
  return pin.has_explicit_checked ? pin.checked : checked_ids.contains(state_id);
}

void AddItemHit(std::vector<TrackerPackHitTarget>& targets,
                const Rect& rect,
                std::string_view code) {
  if (code.empty() || rect.width <= 0 || rect.height <= 0) {
    return;
  }
  targets.push_back(TrackerPackHitTarget{
      .kind = TrackerPackHitTargetKind::Item,
      .x = rect.x,
      .y = rect.y,
      .width = rect.width,
      .height = rect.height,
      .code = std::string(code),
      .label = std::string(code),
  });
}

void AddPinHit(std::vector<TrackerPackHitTarget>& targets,
               const BundlePinRenderMetadata& pin,
               int center_x,
               int center_y) {
  const int hit_size = std::max(18, static_cast<int>(std::min(28.0, std::max(0.0, pin.size))));
  TrackerPackHitTarget target{
      .kind = TrackerPackHitTargetKind::Pin,
      .x = center_x - hit_size / 2,
      .y = center_y - hit_size / 2,
      .width = hit_size,
      .height = hit_size,
      .pin_id = pin.id,
      .location_id = pin.location_id,
      .group_id = pin.group_id,
      .label = pin.label.empty() ? pin.id : pin.label,
  };
  for (const auto& segment : pin.segments) {
    for (const auto& check : segment.checks) {
      target.checks.push_back(TrackerPackHitCheck{
          .location_id = check.location_id,
          .label = check.label.empty() ? segment.label : check.label,
          .checked = check.checked,
      });
    }
  }
  if (target.checks.empty() && !pin.location_id.empty()) {
    target.checks.push_back(TrackerPackHitCheck{
        .location_id = pin.location_id,
        .label = target.label,
        .checked = pin.checked,
    });
  }
  targets.push_back(std::move(target));
}

void CollectNodeHits(std::vector<TrackerPackHitTarget>& targets,
                     const PackStateContext& context,
                     const nlohmann::json& node,
                     Rect rect,
                     const TrackerOverlayAssetResolver* asset_resolver) {
  if (!node.is_object() || rect.width <= 0 || rect.height <= 0 || ShouldSuppressPackLayoutNode(node)) {
    return;
  }
  rect = ApplyMargins(rect, ParseMargins(JsonStringFlexible(node, "margin")));
  rect = ApplySizeConstraints(rect, node);
  if (rect.width <= 0 || rect.height <= 0) {
    return;
  }
  if (const auto resolved_layout = ResolveLayoutReference(context, node); resolved_layout.has_value()) {
    CollectNodeHits(targets, context, *resolved_layout, rect, asset_resolver);
    return;
  }

  const auto type = node.value("type", std::string{});
  if (type == "container") {
    CollectNodeHits(targets, context, node.value("content", nlohmann::json::object()), rect, asset_resolver);
    return;
  }
  if (type == "group") {
    const auto header = node.value("header", std::string{});
    CollectNodeHits(targets,
                    context,
                    node.value("content", nlohmann::json::object()),
                    Inset(Rect{rect.x, rect.y + (header.empty() ? 0 : 18), rect.width,
                               rect.height - (header.empty() ? 0 : 18)},
                          4),
                    asset_resolver);
    return;
  }
  if (type == "dock") {
    auto remaining = rect;
    std::vector<nlohmann::json> fill_nodes;
    for (const auto& child : NodeContentArray(node)) {
      const auto dock = child.value("dock", std::string{});
      if (dock.empty()) {
        fill_nodes.push_back(child);
        continue;
      }
      Rect child_rect = remaining;
      const auto preferred = EstimateNodePreferredSize(context, child);
      if (dock == "left") {
        child_rect.width = std::min(remaining.width, std::max(120, preferred.width));
        remaining.x += child_rect.width;
        remaining.width -= child_rect.width;
      } else if (dock == "right") {
        child_rect.width = std::min(remaining.width, std::max(120, preferred.width));
        child_rect.x = remaining.x + remaining.width - child_rect.width;
        remaining.width -= child_rect.width;
      } else if (dock == "top") {
        child_rect.height = std::min(remaining.height, std::max(64, preferred.height));
        remaining.y += child_rect.height;
        remaining.height -= child_rect.height;
      } else if (dock == "bottom") {
        child_rect.height = std::min(remaining.height, std::max(64, preferred.height));
        child_rect.y = remaining.y + remaining.height - child_rect.height;
        remaining.height -= child_rect.height;
      } else {
        fill_nodes.push_back(child);
        continue;
      }
      CollectNodeHits(targets, context, child, child_rect, asset_resolver);
    }
    if (!fill_nodes.empty()) {
      const int slot_height = remaining.height / static_cast<int>(fill_nodes.size());
      int cursor_y = remaining.y;
      for (std::size_t index = 0; index < fill_nodes.size(); ++index) {
        Rect child_rect = remaining;
        child_rect.y = cursor_y;
        child_rect.height =
            index + 1 == fill_nodes.size() ? remaining.y + remaining.height - cursor_y : slot_height;
        CollectNodeHits(targets, context, fill_nodes[index], child_rect, asset_resolver);
        cursor_y += slot_height;
      }
    }
    return;
  }
  if (type == "array") {
    const auto children = NodeContentArray(node);
    if (children.empty()) {
      return;
    }
    const auto orientation = node.value("orientation", std::string{"vertical"});
    std::vector<int> preferred_sizes;
    int total_preferred = 0;
    for (const auto& child : children) {
      const auto preferred = EstimateNodePreferredSize(context, child);
      const int axis_size = orientation == "horizontal" ? preferred.width : preferred.height;
      preferred_sizes.push_back(std::max(0, axis_size));
      total_preferred += std::max(0, axis_size);
    }
    const int available = orientation == "horizontal" ? rect.width : rect.height;
    int cursor_x = rect.x;
    int cursor_y = rect.y;
    int consumed = 0;
    for (std::size_t index = 0; index < children.size(); ++index) {
      const int allocated = index + 1 == children.size()
                                ? std::max(0, available - consumed)
                                : total_preferred > 0
                                      ? std::max(0, static_cast<int>(
                                                       static_cast<long long>(preferred_sizes[index]) * available /
                                                       std::max(1, total_preferred)))
                                      : available / static_cast<int>(children.size());
      Rect child_rect = rect;
      if (orientation == "horizontal") {
        child_rect.x = cursor_x;
        child_rect.width = allocated;
        cursor_x += allocated;
      } else {
        child_rect.y = cursor_y;
        child_rect.height = allocated;
        cursor_y += allocated;
      }
      consumed += allocated;
      CollectNodeHits(targets, context, children[index], child_rect, asset_resolver);
    }
    return;
  }
  if (type == "itemgrid") {
    const auto rows = node.value("rows", nlohmann::json::array());
    int max_columns = 0;
    for (const auto& row : rows) {
      if (row.is_array()) {
        max_columns = std::max(max_columns, static_cast<int>(row.size()));
      }
    }
    if (!rows.is_array() || rows.empty() || max_columns <= 0) {
      return;
    }
    const int desired_w = JsonIntFlexible(node, "item_width", JsonIntFlexible(node, "item_size", 16));
    const int desired_h = JsonIntFlexible(node, "item_height", JsonIntFlexible(node, "item_size", 16));
    const auto item_margins = ParseMargins(JsonStringFlexible(node, "item_margin"));
    const int gap_x = std::max(0, item_margins.left + item_margins.right);
    const int gap_y = std::max(0, item_margins.top + item_margins.bottom);
    const int cell_width = std::max(10, std::min(desired_w, (rect.width - gap_x * (max_columns - 1)) / max_columns));
    const int cell_height = std::max(10, std::min(desired_h, (rect.height - gap_y * (static_cast<int>(rows.size()) - 1)) /
                                                            std::max(1, static_cast<int>(rows.size()))));
    const int content_width = cell_width * max_columns + gap_x * std::max(0, max_columns - 1);
    const int content_height = cell_height * static_cast<int>(rows.size()) +
                               gap_y * std::max(0, static_cast<int>(rows.size()) - 1);
    const int grid_origin_x = ResolveAlignedOrigin(rect.x, rect.width, content_width, JsonStringFlexible(node, "h_alignment"));
    const int grid_origin_y = ResolveAlignedOrigin(rect.y, rect.height, content_height, JsonStringFlexible(node, "v_alignment"));
    for (std::size_t row_index = 0; row_index < rows.size(); ++row_index) {
      const auto& row = rows[row_index];
      if (!row.is_array()) {
        continue;
      }
      const int row_columns = static_cast<int>(row.size());
      const int row_width = cell_width * row_columns + gap_x * std::max(0, row_columns - 1);
      const int row_origin_x = ResolveAlignedOrigin(grid_origin_x, content_width, row_width, JsonStringFlexible(node, "item_h_alignment"));
      for (std::size_t col_index = 0; col_index < row.size(); ++col_index) {
        if (!row[col_index].is_string()) {
          continue;
        }
        const auto code = row[col_index].get<std::string>();
        if (code.empty()) {
          continue;
        }
        AddItemHit(targets,
                   Rect{row_origin_x + static_cast<int>(col_index) * (cell_width + gap_x),
                        grid_origin_y + static_cast<int>(row_index) * (cell_height + gap_y),
                        cell_width,
                        cell_height},
                   code);
      }
    }
    return;
  }
  if (type == "item") {
    AddItemHit(targets, rect, JsonStringFlexible(node, "item"));
    return;
  }
  if (type == "map") {
    std::string map_id = context.resolved.active_map_id;
    std::string selected_pack_map;
    if (const auto maps = node.value("maps", nlohmann::json::array()); maps.is_array() && !maps.empty()) {
      for (const auto& entry : maps) {
        if (!entry.is_string()) {
          continue;
        }
        const auto pack_map = entry.get<std::string>();
        std::string candidate;
        if (ResolveContextPackMapId(context, pack_map, &candidate) && candidate == context.resolved.active_map_id) {
          selected_pack_map = pack_map;
          map_id = candidate;
          break;
        }
        if (selected_pack_map.empty()) {
          selected_pack_map = pack_map;
          map_id = candidate;
        }
      }
    }
    if (selected_pack_map.empty()) {
      selected_pack_map = ResolveDominantContextPackMapName(context, map_id);
    }
    auto image_rect = Inset(rect, 4);
    std::optional<TrackerOverlayResolvedAsset> image;
    const auto pack_map_asset = ResolveContextPackMapAsset(context, map_id, selected_pack_map);
    if (!pack_map_asset.empty()) {
      image = ResolveAsset(asset_resolver, pack_map_asset);
    }
    const auto* active_map = context.bundle != nullptr ? context.bundle->FindMap(map_id) : nullptr;
    if (!image.has_value() && active_map != nullptr && active_map->raster_image.has_value()) {
      image = TrackerOverlayResolvedAsset{active_map->raster_image->width,
                                          active_map->raster_image->height,
                                          active_map->raster_image->rgba_pixels.data()};
    } else if (!image.has_value() && active_map != nullptr && !active_map->image.empty()) {
      image = ResolveAsset(asset_resolver, active_map->image);
    }
    unsigned source_width = 512;
    unsigned source_height = 512;
    if (image.has_value() && image->width > 0 && image->height > 0) {
      source_width = image->width;
      source_height = image->height;
    } else if (active_map != nullptr && active_map->raster_image.has_value() &&
               active_map->raster_image->width > 0 && active_map->raster_image->height > 0) {
      source_width = active_map->raster_image->width;
      source_height = active_map->raster_image->height;
    }
    const double aspect = static_cast<double>(source_width) / static_cast<double>(source_height);
    int draw_width = image_rect.width;
    int draw_height = std::max(1, static_cast<int>(draw_width / aspect));
    if (draw_height > image_rect.height) {
      draw_height = image_rect.height;
      draw_width = std::max(1, static_cast<int>(draw_height * aspect));
    }
    image_rect.x += (image_rect.width - draw_width) / 2;
    image_rect.y += (image_rect.height - draw_height) / 2;
    image_rect.width = draw_width;
    image_rect.height = draw_height;
    const auto checked_ids = CheckedLocationIds(context.runtime);
    for (const auto& pin : BuildBundlePins(context.runtime, context.resolved)) {
      if (!selected_pack_map.empty()) {
        if (pin.pack_map.empty() || CanonicalToken(pin.pack_map) != CanonicalToken(selected_pack_map)) {
          continue;
        }
      } else if (!map_id.empty() && (pin.map_id.empty() || pin.map_id != map_id)) {
        continue;
      }
      auto hit_pin = pin;
      hit_pin.checked = PinChecked(pin, checked_ids);
      if (hit_pin.checked) {
        continue;
      }
      AddPinHit(targets,
                hit_pin,
                ResolveMapCoordinate(pin.x, image_rect.x, image_rect.width, source_width),
                ResolveMapCoordinate(pin.y, image_rect.y, image_rect.height, source_height));
    }
    return;
  }
  if (type == "tabbed") {
    const auto tabs = node.value("tabs", nlohmann::json::array());
    if (!tabs.is_array() || tabs.empty()) {
      return;
    }
    std::size_t active_index = 0;
    bool matched_active_tab = false;
    for (std::size_t index = 0; index < tabs.size(); ++index) {
      if (tabs[index].is_object() && NodeTargetsActiveTrackerTab(context, tabs[index])) {
        active_index = index;
        matched_active_tab = true;
        break;
      }
    }
    if (!matched_active_tab) {
      for (std::size_t index = 0; index < tabs.size(); ++index) {
        if (tabs[index].is_object() && NodeTargetsActiveMap(context, tabs[index].value("content", nlohmann::json::object()))) {
          active_index = index;
          break;
        }
      }
    }
    CollectNodeHits(targets, context, tabs[active_index].value("content", nlohmann::json::object()), rect, asset_resolver);
  }
}

}  // namespace

std::vector<TrackerPackHitTarget> BuildPackLayoutHitTargets(
    const TrackerRuntime& runtime,
    const TrackerResolvedViewState& resolved,
    int x,
    int y,
    int width,
    int height,
    const TrackerOverlayAssetResolver* asset_resolver) {
  const auto* bundle = runtime.Bundle();
  PackLayoutDocument snapshot_document;
  const bool use_snapshot_layouts = LoadSnapshotLayoutDefinitions(runtime.AuthoritativeState().snapshot, snapshot_document);
  if (!use_snapshot_layouts && bundle == nullptr) {
    return {};
  }
  const auto& document = use_snapshot_layouts ? snapshot_document : PackLayoutForBundle(*bundle, !SnapshotProvidesPackVisuals(runtime));
  const auto root_key = SelectLayoutRoot(document, width, height);
  const auto it = document.layouts.find(root_key);
  const auto player_focused_root = BuildPlayerFocusedRoot(document, width, height);
  if (!player_focused_root.has_value() && it == document.layouts.end()) {
    return {};
  }
  PackStateContext context{runtime, resolved, asset_resolver};
  context.bundle = bundle;
  context.document = &document;
  if (context.document == nullptr) {
    return {};
  }
  std::vector<TrackerPackHitTarget> targets;
  CollectNodeHits(targets, context, player_focused_root.has_value() ? *player_focused_root : it->second,
                  Rect{x, y, width, height}, asset_resolver);
  return targets;
}

const TrackerPackHitTarget* FindPackLayoutHitTarget(
    const std::vector<TrackerPackHitTarget>& targets,
    int point_x,
    int point_y) {
  for (auto it = targets.rbegin(); it != targets.rend(); ++it) {
    if (point_x >= it->x && point_x < it->x + it->width &&
        point_y >= it->y && point_y < it->y + it->height) {
      return &*it;
    }
  }
  return nullptr;
}

}  // namespace sekaiemu::spike

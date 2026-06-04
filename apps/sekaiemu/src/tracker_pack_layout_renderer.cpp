#include "tracker_pack_layout_renderer.hpp"

#include "tracker_overlay_render_state.hpp"
#include "tracker_overlay_style.hpp"
#include "tracker_pack_layout_document.hpp"
#include "tracker_pack_layout_engine.hpp"
#include "tracker_pack_layout_json.hpp"
#include "tracker_pack_layout_model.hpp"
#include "tracker_pack_layout_visual_state.hpp"
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
      {"content",
       {
           {"type", "layout"},
           {"key", item_key},
           {"h_alignment", "center"},
           {"v_alignment", "center"},
       }},
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

  return nlohmann::json{
      {"type", "dock"},
      {"content",
       nlohmann::json::array({
           item_group,
           {
               {"type", "layout"},
               {"key", map_key},
           },
       })},
  };
}

bool ShouldShareItemGridRowOrigin(const nlohmann::json& rows, int max_columns) {
  if (!rows.is_array()) {
    return false;
  }
  int array_rows = 0;
  int full_rows = 0;
  for (const auto& row : rows) {
    if (!row.is_array()) {
      continue;
    }
    ++array_rows;
    if (static_cast<int>(row.size()) == max_columns) {
      ++full_rows;
    }
  }
  return (max_columns >= 10 || array_rows >= 7) && full_rows * 2 >= array_rows;
}

void RenderNode(OverlayCanvas& canvas,
                const UiPalette& palette,
                const PackStateContext& context,
                const nlohmann::json& node,
                Rect rect) {
  if (!node.is_object() || rect.width <= 0 || rect.height <= 0) {
    return;
  }
  if (ShouldSuppressPackLayoutNode(node)) {
    return;
  }

  rect = ApplyMargins(rect, ParseMargins(JsonStringFlexible(node, "margin")));
  rect = ApplySizeConstraints(rect, node);
  if (rect.width <= 0 || rect.height <= 0) {
    return;
  }

  if (const auto resolved_layout = ResolveLayoutReference(context, node); resolved_layout.has_value()) {
    RenderNode(canvas, palette, context, *resolved_layout, rect);
    return;
  }

  const auto type = node.value("type", std::string{});
  if (type == "container") {
    RenderNode(canvas, palette, context, node.value("content", nlohmann::json::object()), rect);
    return;
  }

  if (type == "group") {
    const auto header = node.value("header", std::string{});
    DrawFrameWithCustomHeader(canvas,
                              palette,
                              rect,
                              header,
                              JsonStringFlexible(node, "header_background"));
    RenderNode(canvas, palette, context, node.value("content", nlohmann::json::object()),
               Inset(Rect{rect.x, rect.y + (header.empty() ? 0 : 18), rect.width,
                          rect.height - (header.empty() ? 0 : 18)},
                     4));
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
      RenderNode(canvas, palette, context, child, child_rect);
    }
    if (!fill_nodes.empty()) {
      const int slot_height = remaining.height / static_cast<int>(fill_nodes.size());
      int cursor_y = remaining.y;
      for (std::size_t index = 0; index < fill_nodes.size(); ++index) {
        Rect child_rect = remaining;
        child_rect.y = cursor_y;
        child_rect.height = index + 1 == fill_nodes.size()
                                ? remaining.y + remaining.height - cursor_y
                                : slot_height;
        RenderNode(canvas, palette, context, fill_nodes[index], child_rect);
        cursor_y += slot_height;
      }
    }
    return;
  }

  if (type == "array") {
    const auto orientation = node.value("orientation", std::string{"vertical"});
    const auto children = NodeContentArray(node);
    if (children.empty()) {
      return;
    }
    std::vector<int> preferred_sizes;
    preferred_sizes.reserve(children.size());
    int total_preferred = 0;
    for (const auto& child : children) {
      const auto preferred = EstimateNodePreferredSize(context, child);
      const int axis_size = orientation == "horizontal" ? preferred.width : preferred.height;
      preferred_sizes.push_back(std::max(0, axis_size));
      total_preferred += std::max(0, axis_size);
    }

    const int available = orientation == "horizontal" ? rect.width : rect.height;
    std::vector<int> allocated_sizes(children.size(), 0);
    if (total_preferred > 0) {
      int consumed = 0;
      for (std::size_t index = 0; index < children.size(); ++index) {
        if (index + 1 == children.size()) {
          allocated_sizes[index] = std::max(0, available - consumed);
          break;
        }
        const int scaled =
            std::max(0, static_cast<int>((static_cast<long long>(preferred_sizes[index]) * available) /
                                         std::max(1, total_preferred)));
        allocated_sizes[index] = scaled;
        consumed += scaled;
      }
    } else {
      const int step = available / static_cast<int>(children.size());
      int consumed = 0;
      for (std::size_t index = 0; index < children.size(); ++index) {
        if (index + 1 == children.size()) {
          allocated_sizes[index] = std::max(0, available - consumed);
          break;
        }
        allocated_sizes[index] = step;
        consumed += step;
      }
    }

    int cursor_x = rect.x;
    int cursor_y = rect.y;
    for (std::size_t index = 0; index < children.size(); ++index) {
      Rect child_rect = rect;
      if (orientation == "horizontal") {
        child_rect.x = cursor_x;
        child_rect.width = allocated_sizes[index];
        cursor_x += allocated_sizes[index];
      } else {
        child_rect.y = cursor_y;
        child_rect.height = allocated_sizes[index];
        cursor_y += allocated_sizes[index];
      }
      RenderNode(canvas, palette, context, children[index], child_rect);
    }
    return;
  }

  if (type == "itemgrid") {
    const auto rows = node.value("rows", nlohmann::json::array());
    if (!rows.is_array() || rows.empty()) {
      return;
    }
    int max_columns = 0;
    for (const auto& row : rows) {
      if (row.is_array()) {
        max_columns = std::max(max_columns, static_cast<int>(row.size()));
      }
    }
    if (max_columns <= 0) {
      return;
    }
    const int desired_w = JsonIntFlexible(node, "item_width", JsonIntFlexible(node, "item_size", 16));
    const int desired_h = JsonIntFlexible(node, "item_height", JsonIntFlexible(node, "item_size", 16));
    const int badge_scale = JsonIntFlexible(node, "badge_font_size", 0) >= 12 ? 2 : 1;
    const auto item_margins = ParseMargins(JsonStringFlexible(node, "item_margin"));
    const int gap_x = std::max(0, item_margins.left + item_margins.right);
    const int gap_y = std::max(0, item_margins.top + item_margins.bottom);
    const int cell_width =
        std::max(10, std::min(desired_w, (rect.width - gap_x * (max_columns - 1)) / max_columns));
    const int cell_height =
        std::max(10, std::min(desired_h, (rect.height - gap_y * (static_cast<int>(rows.size()) - 1)) /
                                             std::max(1, static_cast<int>(rows.size()))));
    const int content_width = cell_width * max_columns + gap_x * std::max(0, max_columns - 1);
    const int content_height =
        cell_height * static_cast<int>(rows.size()) +
        gap_y * std::max(0, static_cast<int>(rows.size()) - 1);
    const auto grid_h_alignment = JsonStringFlexible(node, "h_alignment");
    const auto grid_v_alignment = JsonStringFlexible(node, "v_alignment");
    const auto item_v_alignment = JsonStringFlexible(node, "item_v_alignment");
    const bool center_small_vertical_slack =
        grid_v_alignment.empty() && item_v_alignment == "center" && rect.height > content_height &&
        rect.height - content_height <= std::max(2, cell_height / 2);
    const int grid_origin_x =
        ResolveAlignedOrigin(rect.x, rect.width, content_width, grid_h_alignment);
    const int grid_origin_y =
        ResolveAlignedOrigin(rect.y,
                             rect.height,
                             content_height,
                             center_small_vertical_slack ? std::string_view{"center"}
                                                         : std::string_view{grid_v_alignment});
    const bool shared_row_origin = ShouldShareItemGridRowOrigin(rows, max_columns);
    for (std::size_t row_index = 0; row_index < rows.size(); ++row_index) {
      const auto& row = rows[row_index];
      if (!row.is_array()) {
        continue;
      }
      const int row_columns = static_cast<int>(row.size());
      const int row_width = cell_width * row_columns + gap_x * std::max(0, row_columns - 1);
      const int row_origin_x =
          shared_row_origin
              ? grid_origin_x
              : ResolveAlignedOrigin(grid_origin_x,
                                     content_width,
                                     row_width,
                                     JsonStringFlexible(node, "item_h_alignment"));
      for (std::size_t col_index = 0; col_index < row.size(); ++col_index) {
        if (!row[col_index].is_string()) {
          continue;
        }
        const auto code = row[col_index].get<std::string>();
        if (code.empty()) {
          continue;
        }
        Rect cell{row_origin_x + static_cast<int>(col_index) * (cell_width + gap_x),
                  grid_origin_y + static_cast<int>(row_index) * (cell_height + gap_y),
                  cell_width,
                  cell_height};
        DrawPackItemCell(canvas, palette, cell, context, code, badge_scale);
      }
    }
    return;
  }

  if (type == "item") {
    const auto item_code = JsonStringFlexible(node, "item");
    const bool compact = node.value("compact", false);
    DrawPackItemRow(canvas, palette, context, rect, item_code, JsonStringFlexible(node, "label"), compact);
    return;
  }

  if (type == "map") {
    DrawFrame(canvas, palette, rect, {});
    std::string map_id = context.resolved.active_map_id;
    std::string selected_pack_map;
    if (const auto maps = node.value("maps", nlohmann::json::array()); maps.is_array() && !maps.empty()) {
      if (maps.size() == 1 && maps.front().is_string()) {
        selected_pack_map = maps.front().get<std::string>();
        ResolveContextPackMapId(context, selected_pack_map, &map_id);
      } else {
        for (const auto& entry : maps) {
          if (!entry.is_string()) {
            continue;
          }
          const auto pack_map = entry.get<std::string>();
          std::string candidate;
          if (ResolveContextPackMapId(context, pack_map, &candidate) &&
              candidate == context.resolved.active_map_id) {
            map_id = candidate;
            selected_pack_map = pack_map;
            break;
          }
          if (map_id.empty()) {
            map_id = candidate;
            selected_pack_map = pack_map;
          }
        }
      }
    }
    if (selected_pack_map.empty()) {
      selected_pack_map = ResolveDominantContextPackMapName(context, map_id);
    }
    auto image_rect = Inset(rect, 4);
    unsigned source_width = 0;
    unsigned source_height = 0;
    std::optional<TrackerOverlayResolvedAsset> image;
    const auto pack_map_asset = ResolveContextPackMapAsset(context, map_id, selected_pack_map);
    if (!pack_map_asset.empty()) {
      image = ResolveAsset(context.asset_resolver, pack_map_asset);
    }
    const auto* active_map =
        context.bundle != nullptr ? context.bundle->FindMap(map_id) : nullptr;
    if (!image.has_value() && active_map != nullptr && active_map->raster_image.has_value()) {
      image = TrackerOverlayResolvedAsset{active_map->raster_image->width,
                                          active_map->raster_image->height,
                                          active_map->raster_image->rgba_pixels.data()};
    } else if (!image.has_value() && active_map != nullptr && !active_map->image.empty()) {
      image = ResolveAsset(context.asset_resolver, active_map->image);
    }
    if (image.has_value() && image->rgba_pixels != nullptr && image->width > 0 && image->height > 0) {
      source_width = image->width;
      source_height = image->height;
      const double aspect = static_cast<double>(image->width) / static_cast<double>(image->height);
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
      canvas.DrawImage(image_rect.x,
                       image_rect.y,
                       image_rect.width,
                       image_rect.height,
                       image->rgba_pixels,
                       image->width,
                       image->height);
      DrawPackPinsForMap(canvas, palette, context.runtime, context.resolved, image_rect.x, image_rect.y,
                         image_rect.width, image_rect.height, source_width, source_height, map_id,
                         selected_pack_map, context.asset_resolver);
    }
    return;
  }

  if (type == "recentpins") {
    const auto style_name = node.value("style", std::string{"wrap"});
    const auto style = style_name == "stack" ? RecentPinsStyle::Stack : RecentPinsStyle::Wrap;
    DrawRecentPins(canvas, palette, context, Inset(rect, 4), style, node.value("compact", false));
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
      if (!tabs[index].is_object()) {
        continue;
      }
      if (NodeTargetsActiveTrackerTab(context, tabs[index])) {
        active_index = index;
        matched_active_tab = true;
        break;
      }
    }
    if (!matched_active_tab) {
      for (std::size_t index = 0; index < tabs.size(); ++index) {
        if (!tabs[index].is_object()) {
          continue;
        }
        if (NodeTargetsActiveMap(context, tabs[index].value("content", nlohmann::json::object()))) {
          active_index = index;
          break;
        }
      }
    }
    RenderNode(canvas, palette, context, tabs[active_index].value("content", nlohmann::json::object()),
               rect);
    return;
  }
}

}  // namespace

bool RenderPackDrivenTrackerBody(OverlayCanvas& canvas,
                                 const TrackerRuntime& runtime,
                                 const TrackerResolvedViewState& resolved,
                                 int x,
                                 int y,
                                 int width,
                                 int height,
                                 const TrackerOverlayAssetResolver* asset_resolver) {
  const auto* bundle = runtime.Bundle();
  PackLayoutDocument snapshot_document;
  const bool use_snapshot_layouts =
      LoadSnapshotLayoutDefinitions(runtime.AuthoritativeState().snapshot, snapshot_document);
  if (!use_snapshot_layouts && bundle == nullptr) {
    return false;
  }
  const auto& document = use_snapshot_layouts
                             ? snapshot_document
                             : PackLayoutForBundle(*bundle, !SnapshotProvidesPackVisuals(runtime));
  const auto root_key = SelectLayoutRoot(document, width, height);
  const auto it = document.layouts.find(root_key);
  const auto player_focused_root = BuildPlayerFocusedRoot(document, width, height);
  if (!player_focused_root.has_value() && it == document.layouts.end()) {
    return false;
  }

  const UiPalette palette;
  PackStateContext context{runtime, resolved, asset_resolver};
  context.document = &document;
  BuildPackStateContext(context);
  if (context.document == nullptr) {
    return false;
  }

  RenderNode(canvas,
             palette,
             context,
             player_focused_root.has_value() ? *player_focused_root : it->second,
             Rect{x, y, width, height});
  return true;
}

}  // namespace sekaiemu::spike
